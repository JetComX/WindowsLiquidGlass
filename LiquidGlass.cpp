// LiquidGlass - Real-time frosted glass for Windows (D3D11)
#include "LiquidGlass.h"
#include "LiquidGlassShaders.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <wincodec.h>
#include <cstdio>
#include <cstddef>
#include <vector>
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"ole32.lib")

namespace LiquidGlass {
using Microsoft::WRL::ComPtr;

// ============================================================================
// Logging
// ============================================================================
int gFrameCount = 0; // per-process frame counter (non-static = external linkage OK)
#define LG_LOG(fmt, ...) do { \
    char _b[512]; sprintf_s(_b, "[LG:%04d] " fmt "\n", gFrameCount, ##__VA_ARGS__); \
    printf("%s", _b); OutputDebugStringA(_b); \
} while(0)
#define LG_WARN(fmt, ...) do { \
    char _b[512]; sprintf_s(_b, "[LG:%04d] WARN: " fmt "\n", gFrameCount, ##__VA_ARGS__); \
    printf("%s", _b); OutputDebugStringA(_b); \
} while(0)
#define LG_ERR(fmt, ...) do { \
    char _b[512]; sprintf_s(_b, "[LG:%04d] ERROR: " fmt "\n", gFrameCount, ##__VA_ARGS__); \
    printf("%s", _b); OutputDebugStringA(_b); \
} while(0)

// ============================================================================
// D3D11 helpers
// ============================================================================
struct RT {
    ComPtr<ID3D11Texture2D> tex; ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11ShaderResourceView> srv; int w=0,h=0;
    bool Create(ID3D11Device* d,int ww,int hh,DXGI_FORMAT f=DXGI_FORMAT_R8G8B8A8_UNORM){
        Release(); D3D11_TEXTURE2D_DESC dd={}; dd.Width=ww;dd.Height=hh;dd.MipLevels=1;
        dd.ArraySize=1;dd.Format=f;dd.SampleDesc.Count=1;
        dd.Usage=D3D11_USAGE_DEFAULT;dd.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(d->CreateTexture2D(&dd,nullptr,tex.GetAddressOf())))return false;
        d->CreateRenderTargetView(tex.Get(),nullptr,rtv.GetAddressOf());
        d->CreateShaderResourceView(tex.Get(),nullptr,srv.GetAddressOf());
        w=ww;h=hh;return true;
    }
    void Release(){srv.Reset();rtv.Reset();tex.Reset();w=h=0;}
};

// Constant buffer structs — must match HLSL packoffset layout exactly
struct alignas(16) BlurCB{float tsX,tsY;int kr;float sigma;float weights[16];};     // b0: BlurH/BlurV
struct alignas(16) ImageCB{float iw,ih,sw,sh;};                                      // b0: ImageCopy
struct alignas(16) GlassCB{float px,py,sx,sy;float cr[4];float siX,siY;float rh,ra;float de,sat,disp,dark;float tintR,tintG,tintB,tintA;}; // b0: Refr/Disp
struct alignas(16) HighlightCB{float px,py,sx,sy;float cr[4],hc[4];float mx,my,spotRadius,pad;}; // b0: Highlight
struct alignas(16) ShadowCB{float px,py,sx,sy;float cr[4],so[2];float sb,p1;float sc[4],p2;};     // b0: Shadow

struct Renderer::Impl {
    // D3D11 core
    HWND hwnd=nullptr; int width=0,height=0;
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> ctx; ComPtr<IDXGISwapChain> sc;
    // Render targets (blur/glass RTs at rtSize, backbuffer stays window-sized)
    RT backbuffer,bgRT,blurHRT,blurVRT,glassRT;
    // Background
    ComPtr<ID3D11ShaderResourceView> bgImg; int bgImgW=0,bgImgH=0;
    float bgCol[3]={1,1,1}; bool hasBgCol=false;
    GlassConfig cfg; // internal params (modified by fluent setters)
    // Shaders
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> blurH,blurV,refr,disp,shd,img,copy,hl;
    // State objects
    ComPtr<ID3D11SamplerState> samp;
    ComPtr<ID3D11BlendState> alphaBlend;
    ComPtr<ID3D11RasterizerState> rasterScissor;
    // Constant buffers
    ComPtr<ID3D11Buffer> cbBlur,cbGlass,cbShd,cbImg,cbHighlight;
    // Cached WIC factory (reused across image loads)
    ComPtr<IWICImagingFactory> wicFactory;
    // D3D11 debug layer
    ComPtr<ID3D11InfoQueue> iq;
    int frameCount = 0; // per-instance frame counter (was static global)
    int lastBgMode = -1; // 0=color, 1=image, -1=first (was static in ApplyBg)
    int glassCallCount = 0; // (was static in RenderGlass)
    static constexpr int KR=15; // Gaussian kernel radius (31-tap = 1 + 2*15)

    ComPtr<ID3D11PixelShader> CompilePS(const char*s, const char*name){
        ComPtr<ID3DBlob> b,err;
        if(FAILED(D3DCompile(s,strlen(s),nullptr,nullptr,nullptr,"main","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,b.GetAddressOf(),err.GetAddressOf()))){
            if(err)LG_ERR("PS(%s):%s",name,(char*)err->GetBufferPointer());
            else LG_ERR("PS(%s): unknown error",name);
            return nullptr;
        }
        ComPtr<ID3D11PixelShader> ps; device->CreatePixelShader(b->GetBufferPointer(),b->GetBufferSize(),nullptr,ps.GetAddressOf());
        return ps;
    }
    void DrawFS(){ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);ctx->IASetInputLayout(nullptr);ctx->Draw(3,0);}
    void SetRT(RT&rt,float r=0,float g=0,float b=0,float a=1){float c[4]={r,g,b,a};ctx->OMSetRenderTargets(1,rt.rtv.GetAddressOf(),nullptr);ctx->ClearRenderTargetView(rt.rtv.Get(),c);D3D11_VIEWPORT vp={0,0,(float)rt.w,(float)rt.h,0,1};ctx->RSSetViewports(1,&vp);}
    void SetVP(){D3D11_VIEWPORT vp={0,0,(float)width,(float)height,0,1};ctx->RSSetViewports(1,&vp);}

    void ApplyBg(){
        ctx->OMSetRenderTargets(1,backbuffer.rtv.GetAddressOf(),nullptr);
        if(bgImg){
            ImageCB cb={(float)bgImgW,(float)bgImgH,(float)width,(float)height};
            ctx->UpdateSubresource(cbImg.Get(),0,nullptr,&cb,0,0);
            ctx->PSSetConstantBuffers(0,1,cbImg.GetAddressOf());
            ctx->PSSetShader(img.Get(),nullptr,0);
            ctx->PSSetShaderResources(0,1,bgImg.GetAddressOf());
            SetVP();DrawFS();
            ctx->OMSetRenderTargets(1,bgRT.rtv.GetAddressOf(),nullptr);
            DrawFS();
            if(lastBgMode!=1){LG_LOG("ApplyBg SWITCH to IMAGE %dx%d",bgImgW,bgImgH);lastBgMode=1;}
        }else{
            float r=hasBgCol?bgCol[0]:1, g=hasBgCol?bgCol[1]:1, b=hasBgCol?bgCol[2]:1;
            float c[4]={r,g,b,1};
            ctx->ClearRenderTargetView(backbuffer.rtv.Get(),c);
            ctx->OMSetRenderTargets(1,bgRT.rtv.GetAddressOf(),nullptr);
            ctx->ClearRenderTargetView(bgRT.rtv.Get(),c);
            if(lastBgMode!=0){LG_LOG("ApplyBg SWITCH to COLOR (%.2f,%.2f,%.2f)",r,g,b);lastBgMode=0;}
        }
    }
    bool LoadImg(const wchar_t*path){
        if(!wicFactory){
            HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(wicFactory.GetAddressOf()));
            if(FAILED(hr)){LG_ERR("WIC factory failed 0x%08X",hr);return false;}
        }
        ComPtr<IWICBitmapDecoder> dec;
        HRESULT hr=wicFactory->CreateDecoderFromFilename(path,nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,dec.GetAddressOf());
        if(FAILED(hr)){LG_ERR("CreateDecoder failed 0x%08X path=%ls",hr,path);return false;}
        ComPtr<IWICBitmapFrameDecode> frm;
        hr=dec->GetFrame(0,frm.GetAddressOf());
        if(FAILED(hr)){LG_ERR("GetFrame failed 0x%08X",hr);return false;}
        ComPtr<IWICFormatConverter> conv;wicFactory->CreateFormatConverter(conv.GetAddressOf());
        conv->Initialize(frm.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
        UINT iw=0,ih=0;conv->GetSize(&iw,&ih);
        std::vector<BYTE> px(iw*ih*4);conv->CopyPixels(nullptr,iw*4,(UINT)px.size(),px.data());
        D3D11_TEXTURE2D_DESC td={};td.Width=iw;td.Height=ih;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA id={px.data(),iw*4,0};ComPtr<ID3D11Texture2D> t;
        hr=device->CreateTexture2D(&td,&id,t.GetAddressOf());
        if(FAILED(hr)){LG_ERR("CreateTexture2D failed 0x%08X size=%dx%d",hr,iw,ih);return false;}
        bgImg.Reset();device->CreateShaderResourceView(t.Get(),nullptr,bgImg.GetAddressOf());bgImgW=iw;bgImgH=ih;
        LG_LOG("LoadImg OK %dx%d",iw,ih);
        return true;
    }
};

// ============================================================================
// Public API
// ============================================================================
Renderer::Renderer():m(new Impl){LG_LOG("Renderer created");}
Renderer::~Renderer(){LG_LOG("Renderer destroyed");delete m;m=nullptr;}
Renderer::Renderer(Renderer&& other) noexcept : m(other.m) { other.m = nullptr; }
Renderer& Renderer::operator=(Renderer&& other) noexcept { if(this!=&other){delete m;m=other.m;other.m=nullptr;} return *this; }
bool Renderer::Init(HWND hwnd,int w,int h){
    LG_LOG("Init hwnd=%p size=%dx%d",hwnd,w,h);
    m->hwnd=hwnd;m->width=w;m->height=h;
    DXGI_SWAP_CHAIN_DESC sd={};sd.BufferCount=2;sd.BufferDesc.Width=w;sd.BufferDesc.Height=h;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=hwnd;sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags|=D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL fl;HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&sd,m->sc.GetAddressOf(),m->device.GetAddressOf(),&fl,m->ctx.GetAddressOf());
    if(FAILED(hr)){LG_ERR("D3D11CreateDevice failed 0x%08X",hr);return false;}
    // Setup D3D11 info queue for debug messages
    if(SUCCEEDED(m->device.As(&m->iq))){
        LG_LOG("D3D11 debug layer active");
    }
    LG_LOG("Device FL 0x%04X",fl);
    // GPU name
    ComPtr<IDXGIDevice> dxgiDev;
    if(SUCCEEDED(m->device.As(&dxgiDev))){
        ComPtr<IDXGIAdapter> adapter;
        if(SUCCEEDED(dxgiDev->GetAdapter(adapter.GetAddressOf()))){
            DXGI_ADAPTER_DESC desc;
            if(SUCCEEDED(adapter->GetDesc(&desc)))
                LG_LOG("GPU: %ls VRAM:%zuMB",desc.Description,desc.DedicatedVideoMemory/1048576);
        }
    }
    ComPtr<ID3D11Texture2D> bb;m->sc->GetBuffer(0,IID_PPV_ARGS(bb.GetAddressOf()));m->device->CreateRenderTargetView(bb.Get(),nullptr,m->backbuffer.rtv.GetAddressOf());m->backbuffer.w=w;m->backbuffer.h=h;
    m->bgRT.Create(m->device.Get(),w,h);m->blurHRT.Create(m->device.Get(),w,h);m->blurVRT.Create(m->device.Get(),w,h);m->glassRT.Create(m->device.Get(),w,h);
    LG_LOG("RTs created: bg blrH blrV %dx%d",w,h);
    ComPtr<ID3DBlob> vb;D3DCompile(FullscreenVS,strlen(FullscreenVS),nullptr,nullptr,nullptr,"main","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,vb.GetAddressOf(),nullptr);m->device->CreateVertexShader(vb->GetBufferPointer(),vb->GetBufferSize(),nullptr,m->vs.GetAddressOf());
    m->blurH=m->CompilePS(BlurH_PS,"BlurH");m->blurV=m->CompilePS(BlurV_PS,"BlurV");m->refr=m->CompilePS(GlassRefractionPS,"Refr");m->disp=m->CompilePS(GlassDispersionPS,"Disp");m->shd=m->CompilePS(ShadowPS,"Shadow");m->hl=m->CompilePS(HighlightPS,"Highlight");m->img=m->CompilePS(ImageCopyPS,"Image");m->copy=m->CompilePS(PassthroughPS,"Passthru");
    if(!m->blurH||!m->blurV||!m->refr||!m->disp||!m->shd||!m->hl||!m->img||!m->copy){LG_ERR("Shader compilation failed");return false;}
    LG_LOG("All 8 shaders OK");
    D3D11_SAMPLER_DESC sm={};sm.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sm.AddressU=sm.AddressV=sm.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;m->device->CreateSamplerState(&sm,m->samp.GetAddressOf());
    D3D11_BLEND_DESC bd={};bd.RenderTarget[0].BlendEnable=TRUE;bd.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;bd.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;bd.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;bd.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;bd.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ONE;bd.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;bd.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;m->device->CreateBlendState(&bd,m->alphaBlend.GetAddressOf());
    D3D11_RASTERIZER_DESC rd={};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.ScissorEnable=TRUE;rd.DepthClipEnable=FALSE;
    m->device->CreateRasterizerState(&rd,m->rasterScissor.GetAddressOf());
    D3D11_BUFFER_DESC cbd={};cbd.Usage=D3D11_USAGE_DEFAULT;cbd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    cbd.ByteWidth=sizeof(BlurCB);m->device->CreateBuffer(&cbd,nullptr,m->cbBlur.GetAddressOf());
    cbd.ByteWidth=sizeof(GlassCB);m->device->CreateBuffer(&cbd,nullptr,m->cbGlass.GetAddressOf());


    cbd.ByteWidth=sizeof(ShadowCB);m->device->CreateBuffer(&cbd,nullptr,m->cbShd.GetAddressOf());
    cbd.ByteWidth=sizeof(HighlightCB);m->device->CreateBuffer(&cbd,nullptr,m->cbHighlight.GetAddressOf());
    cbd.ByteWidth=sizeof(ImageCB);m->device->CreateBuffer(&cbd,nullptr,m->cbImg.GetAddressOf());
    LG_LOG("Init complete %dx%d",w,h);
    return true;
}
void Renderer::Resize(int w,int h){
    if(w==m->width&&h==m->height)return;
    LG_LOG("Resize %dx%d -> %dx%d",m->width,m->height,w,h);
    m->width=w;m->height=h;m->ctx->OMSetRenderTargets(0,nullptr,nullptr);
    m->backbuffer.Release();m->bgRT.Release();m->blurHRT.Release();m->blurVRT.Release();m->glassRT.Release();
    m->sc->ResizeBuffers(2,w,h,DXGI_FORMAT_R8G8B8A8_UNORM,0);
    ComPtr<ID3D11Texture2D> bb;m->sc->GetBuffer(0,IID_PPV_ARGS(bb.GetAddressOf()));m->device->CreateRenderTargetView(bb.Get(),nullptr,m->backbuffer.rtv.GetAddressOf());m->backbuffer.w=w;m->backbuffer.h=h;
    m->bgRT.Create(m->device.Get(),w,h);m->blurHRT.Create(m->device.Get(),w,h);m->blurVRT.Create(m->device.Get(),w,h);m->glassRT.Create(m->device.Get(),w,h);
}
void Renderer::Shutdown(){LG_LOG("Shutdown");delete m;m=nullptr;}
void Renderer::BeginFrame(){
    gFrameCount++;
    float bf[4]={1,1,1,1};D3D11_VIEWPORT vp={0,0,(float)m->width,(float)m->height,0,1};m->ctx->RSSetViewports(1,&vp);m->ctx->VSSetShader(m->vs.Get(),nullptr,0);m->ctx->GSSetShader(nullptr,nullptr,0);m->ctx->PSSetSamplers(0,1,m->samp.GetAddressOf());m->ctx->OMSetBlendState(nullptr,bf,0xFFFFFFFF);m->ApplyBg();
}
void Renderer::EndFrame(){
    HRESULT hr=m->sc->Present(1,0);
    if(FAILED(hr)){
        LG_ERR("Present failed HRESULT=0x%08X",hr);
        if(hr==DXGI_ERROR_DEVICE_REMOVED){
            HRESULT dr=m->device->GetDeviceRemovedReason();
            LG_ERR("Device removed! Reason=0x%08X",dr);
        }
    }
    if(gFrameCount<=2) DumpDebugMessages(); // Only first 2 frames for D3D11 validation
}
void Renderer::SetBackgroundColor(float r,float g,float b){
    LG_LOG("SetBackgroundColor(%.2f,%.2f,%.2f)",r,g,b);
    m->bgCol[0]=r;m->bgCol[1]=g;m->bgCol[2]=b;m->hasBgCol=true;m->bgImg.Reset();
}
bool Renderer::LoadBackgroundImage(const wchar_t*p){
    LG_LOG("LoadBackgroundImage: %ls",p);
    m->hasBgCol=false;
    bool ok=m->LoadImg(p);
    if(ok)LG_LOG("Image loaded %dx%d",m->bgImgW,m->bgImgH);
    else LG_ERR("Image load FAILED");
    return ok;
}
void Renderer::ClearBackground(){LG_LOG("ClearBackground");m->bgImg.Reset();m->hasBgCol=false;}

// ---- 玻璃参数 setter（链式调用，返回 *this） ----
Renderer& Renderer::Blur(float s)               { LG_LOG("Set Blur=%.2f",s); m->cfg.blurSigma = s; return *this; }
Renderer& Renderer::Saturation(float s)         { LG_LOG("Set Saturation=%.2f",s); m->cfg.saturation = s; return *this; }
Renderer& Renderer::RefractionHeight(float h)    { LG_LOG("Set RefractionHeight=%.0f",h); m->cfg.refractionHeight = h; return *this; }
Renderer& Renderer::RefrAmountCorrect(float a)  { if(a>=0){m->cfg.refractionCorrect=a;} return *this; }
Renderer& Renderer::RefrAmountNegative(float a)  { if(a>=0){m->cfg.refractionNegative=a;} return *this; }
Renderer& Renderer::HighlightAlpha(float alpha){ LG_LOG("Set HighlightAlpha=%.2f",alpha); m->cfg.highlightAlpha = alpha; return *this; }
Renderer& Renderer::Darkening(float v){ LG_LOG("Set Darkening=%.2f",v); m->cfg.darkening = v; return *this; }
Renderer& Renderer::ShadowOffset(float x, float y){ LG_LOG("Set ShadowOffset=(%.0f,%.0f)",x,y); m->cfg.shadowOffsetX=x; m->cfg.shadowOffsetY=y; return *this; }
Renderer& Renderer::ShadowAlpha(float alpha){ LG_LOG("Set ShadowAlpha=%.2f",alpha); m->cfg.shadowAlpha=alpha; return *this; }
Renderer& Renderer::GlassTint(float r, float g, float b, float a){ LG_LOG("Set GlassTint rgba=(%.2f,%.2f,%.2f,%.2f)",r,g,b,a); m->cfg.glassTintR=r;m->cfg.glassTintG=g;m->cfg.glassTintB=b;m->cfg.glassTintA=a;return *this; }
Renderer& Renderer::Radius(float r)             { LG_LOG("Set Radius=%.0f",r); m->cfg.cornerRadius = r; return *this; }
Renderer& Renderer::Dispersion(float intensity) { LG_LOG("Set Dispersion=%.2f",intensity); m->cfg.dispersion = intensity; return *this; }
Renderer& Renderer::Depth(bool on)              { LG_LOG("Set Depth=%d",on); m->cfg.depthEffect = on; return *this; }
Renderer& Renderer::Config(const GlassConfig& c){ LG_LOG("Set Config(bulk)"); m->cfg = c; return *this; }

// 使用内部已设置的参数
void Renderer::RenderGlass(float x,float y,float w,float h){
    RenderGlass(x, y, w, h, m->cfg);
}

void Renderer::RenderGlass(float x,float y,float w,float h,const GlassConfig&c){
    Impl*g=m;float bf[4]={1,1,1,1};
    g->glassCallCount++;
    float glassMin = w < h ? w : h;
    float refrH = c.refractionHeight * glassMin * 0.5f;
    float refrA = (c.refractionCorrect - c.refractionNegative) * glassMin;
#ifdef _DEBUG
    if(g->glassCallCount<=3)
        LG_LOG("RenderGlass pos=(%.0f,%.0f) size=%.0fx%.0f blur=%.1f refrH=%.0f(%.2f) refrA=%.0f(+%.2f-%.2f) r=%.0f sat=%.2f disp=%.2f depth=%d",
            x,y,w,h,c.blurSigma,refrH,c.refractionHeight,refrA,c.refractionCorrect,c.refractionNegative,c.cornerRadius,c.saturation,c.dispersion,c.depthEffect);
#endif
    // Blur pass — precompute Gaussian weights on CPU
    BlurCB bcb={1.f/g->width,1.f/g->height,Impl::KR,c.blurSigma,{}};
    { float sumW=1.f; for(int i=1;i<=Impl::KR;i++) sumW+=2.f*expf(-float(i*i)/(2.f*c.blurSigma*c.blurSigma)); bcb.weights[0]=1.f/sumW;
      for(int i=1;i<=Impl::KR;i++) bcb.weights[i]=expf(-float(i*i)/(2.f*c.blurSigma*c.blurSigma))/sumW; }
    g->ctx->UpdateSubresource(g->cbBlur.Get(),0,nullptr,&bcb,0,0);
    g->ctx->PSSetConstantBuffers(0,1,g->cbBlur.GetAddressOf());
    g->ctx->PSSetShader(g->blurH.Get(),nullptr,0);
    // Set new RT first (unbinds old RT), THEN bind old RT's SRV
    g->SetRT(g->blurHRT);
    g->ctx->PSSetShaderResources(0,1,g->bgRT.srv.GetAddressOf());
    g->DrawFS();
    g->ctx->PSSetShader(g->blurV.Get(),nullptr,0);
    g->SetRT(g->blurVRT);
    g->ctx->PSSetShaderResources(0,1,g->blurHRT.srv.GetAddressOf());
    g->DrawFS();
    // Glass offscreen RT (pre-created in Init/Resize)
    g->SetRT(g->glassRT,0,0,0,0); // transparent clear
    float r=c.cornerRadius,rad[4]={r,r,r,r};
    GlassCB gc={x,y,w,h,{rad[0],rad[1],rad[2],rad[3]},1.f/g->width,1.f/g->height,refrH,refrA,c.depthEffect?1.f:0,c.saturation,c.dispersion,c.darkening,c.glassTintR,c.glassTintG,c.glassTintB,c.glassTintA};
    if(c.glassTintA>0.0f) { static float lastTintA=-1; if(fabsf(c.glassTintA-lastTintA)>0.01f) {
        LG_LOG("GlassTint active: rgba=(%.2f,%.2f,%.2f,%.2f)", c.glassTintR,c.glassTintG,c.glassTintB,c.glassTintA); lastTintA=c.glassTintA; }}
    g->ctx->UpdateSubresource(g->cbGlass.Get(),0,nullptr,&gc,0,0);
#ifdef _DEBUG
    if(g->glassCallCount<=3){
        LG_LOG("GlassCB upload: pos=(%.0f,%.0f) size=(%.0f,%.0f) radii=(%.0f,%.0f,%.0f,%.0f)",
            gc.px,gc.py,gc.sx,gc.sy,gc.cr[0],gc.cr[1],gc.cr[2],gc.cr[3]);
        LG_LOG("GlassCB upload: screenInv=(%.6f,%.6f) refrH=%.1f refrA=%.1f depth=%.1f sat=%.2f disp=%.2f dark=%.2f tint=(%.2f,%.2f,%.2f,%.2f)",
            gc.siX,gc.siY,gc.rh,gc.ra,gc.de,gc.sat,gc.disp,gc.dark,gc.tintR,gc.tintG,gc.tintB,gc.tintA);
        LG_LOG("GlassCB upload: sizeof=%zu tintOffset=%zu",sizeof(GlassCB),offsetof(GlassCB,tintR));
    }
#endif
    // Scissor rect — clip to glass area + padding
    LONG pad=(LONG)(fabsf(refrA)+12); // 12 = fixed shadow blur
    D3D11_RECT sr={(LONG)(x-c.shadowOffsetX)-pad,(LONG)(y-c.shadowOffsetY)-pad,(LONG)(x+w+c.shadowOffsetX)+pad,(LONG)(y+h+c.shadowOffsetY)+pad};
    if(sr.left<0)sr.left=0;if(sr.top<0)sr.top=0;
    if(sr.right>g->width)sr.right=g->width;if(sr.bottom>g->height)sr.bottom=g->height;
    g->ctx->RSSetScissorRects(1,&sr);
    g->ctx->RSSetState(g->rasterScissor.Get());
    // 1. Shadow pass (alpha blend to glassRT)
    ShadowCB sc={x,y,w,h,{rad[0],rad[1],rad[2],rad[3]},{c.shadowOffsetX,c.shadowOffsetY},0,0,{0,0,0,c.shadowAlpha},0};
    g->ctx->UpdateSubresource(g->cbShd.Get(),0,nullptr,&sc,0,0);
    g->ctx->PSSetConstantBuffers(0,1,g->cbShd.GetAddressOf());
    g->ctx->OMSetBlendState(g->alphaBlend.Get(),bf,0xFFFFFFFF);
    g->ctx->PSSetShader(g->shd.Get(),nullptr,0);
    g->DrawFS();
    // Glass body pass (reads blurVRT for refraction)
    g->ctx->PSSetConstantBuffers(0,1,g->cbGlass.GetAddressOf());
    g->ctx->PSSetShader(c.dispersion>0.0f?g->disp.Get():g->refr.Get(),nullptr,0);
    g->ctx->PSSetShaderResources(0,1,g->blurVRT.srv.GetAddressOf());
    g->DrawFS();
    // 2. Highlight pass (mouse spotlight)
    if(c.highlightAlpha>0.0f) {
        float hcol[4]={1,1,1,c.highlightAlpha};
        HighlightCB hc={x,y,w,h,{rad[0],rad[1],rad[2],rad[3]},{hcol[0],hcol[1],hcol[2],hcol[3]},c.highlightMouseX,c.highlightMouseY,c.spotRadius,0};
        g->ctx->UpdateSubresource(g->cbHighlight.Get(),0,nullptr,&hc,0,0);
        g->ctx->PSSetConstantBuffers(0,1,g->cbHighlight.GetAddressOf());
        g->ctx->PSSetShader(g->hl.Get(),nullptr,0);
        g->DrawFS();
    }
    // 3. Composite pass (alpha blend glassRT -> backbuffer)
    g->ctx->RSSetState(nullptr);
    g->ctx->OMSetRenderTargets(1,g->backbuffer.rtv.GetAddressOf(),nullptr);
    g->SetVP();
    g->ctx->OMSetBlendState(g->alphaBlend.Get(),bf,0xFFFFFFFF);
    g->ctx->PSSetShader(g->copy.Get(),nullptr,0);
    g->ctx->PSSetShaderResources(0,1,g->glassRT.srv.GetAddressOf());
    g->DrawFS();
    g->ctx->OMSetBlendState(nullptr,bf,0xFFFFFFFF);
#ifdef _DEBUG
    if(g->glassCallCount<=3){
        LG_LOG("=== Pipeline step-by-step ===");
        LG_LOG("  1.Blur: sigma=%.1f kernelRadius=%d bgRT(%dx%d)->blurHRT->blurVRT",
            c.blurSigma,Impl::KR,g->bgRT.w,g->bgRT.h);
        LG_LOG("  2.Shadow: offset=(%.0f,%.0f) alpha=%.2f",
            c.shadowOffsetX,c.shadowOffsetY,c.shadowAlpha);
        LG_LOG("  3.GlassBody: refrH=%.0f refrA=%.0f sat=%.2f disp=%.2f depth=%d",
            refrH,refrA,c.saturation,c.dispersion,c.depthEffect);
        LG_LOG("  4.Composite: glassRT(%dx%d)->backbuffer(%dx%d) blend=alpha",
            g->glassRT.w,g->glassRT.h,g->backbuffer.w,g->backbuffer.h);
        LG_LOG("  BG: img=%d col=%d colVal=(%.2f,%.2f,%.2f) imgSize=%dx%d",
            g->bgImg!=nullptr,g->hasBgCol,g->bgCol[0],g->bgCol[1],g->bgCol[2],g->bgImgW,g->bgImgH);
        LG_LOG("  Glass: pos=(%.0f,%.0f) size=%.0fx%.0f radius=%.0f",x,y,w,h,c.cornerRadius);
    }
#endif
}
ID3D11Device* Renderer::GetDevice()const{return m->device.Get();}
ID3D11DeviceContext* Renderer::GetContext()const{return m->ctx.Get();}
int Renderer::Width()const{return m->width;}
int Renderer::Height()const{return m->height;}
bool Renderer::HasBackgroundColor()const{return m->hasBgCol;}
void Renderer::GetBackgroundColor(float&r,float&g,float&b)const{r=m->bgCol[0];g=m->bgCol[1];b=m->bgCol[2];}
void Renderer::DumpDebugMessages(){
    if(!m->iq)return;
    UINT64 n=m->iq->GetNumStoredMessages();
    if(n>0)LG_LOG("D3D11 messages: %llu",n);
    for(UINT64 i=0;i<n;i++){
        SIZE_T l=0;m->iq->GetMessage(i,nullptr,&l);
        if(!l)continue;
        auto*msg=(D3D11_MESSAGE*)_alloca(l);
        if(SUCCEEDED(m->iq->GetMessage(i,msg,&l)))
            LG_WARN("D3D11: %s",msg->pDescription);
    }
    m->iq->ClearStoredMessages();
}

} // namespace LiquidGlass
