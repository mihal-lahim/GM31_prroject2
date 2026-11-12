cbuffer Forest : register(b7) {
    matrix ViewProjection;
    float4 EyeTime;
    float4 Tile; // x,z,size,mode (0 grass, 1 ground, 2 trunk, 3 crown)
    float4 Options; // wind, fog, unused, unused
};
struct Out { float4 position : SV_POSITION; float3 world : TEXCOORD0; float3 normal : TEXCOORD1; float3 color : COLOR0; };
float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
float path(float z) { return sin(z*.065)*3; }
Out main(uint vertex : SV_VertexID, uint instance : SV_InstanceID) {
    Out o;
    float3 p, n=float3(0,1,0), color;
    float2 corners[6]={float2(0,0),float2(0,1),float2(1,0),float2(1,0),float2(0,1),float2(1,1)};
    float2 uv=corners[vertex%6];
    float2 seed=float2(instance,instance*.317)+Tile.xy*13.7;
    float a=hash(seed), b=hash(seed+31.4), c=hash(seed+74.1);
    float2 root=Tile.xy+float2(a,b)*Tile.z;
    if(Tile.w<.5) {
        float segment=floor(vertex/6);
        float t=(segment+uv.y)/2;
        float angle=c*6.283185;
        float2 side=float2(cos(angle),sin(angle));
        float height=.32+c*.55;
        float edge=smoothstep(1.5,2.5,abs(root.x-path(root.y)));
        float bend=Options.x*(sin(EyeTime.w*1.7+root.x*.23+root.y*.18)*.17+sin(EyeTime.w*3.1+c*9)*.045);
        p=float3(root.x,height*t*edge,root.y);
        p.xz+=(side*((uv.x-.5)*.075*(1-t))+float2(.8,.4)*(bend+.16)*t*t)*edge;
        n=normalize(float3(-side.y,.5,side.x));
        color=lerp(float3(.035,.075,.018),float3(.30,.43,.09),t)*(.75+c*.45);
    } else if(Tile.w<1.5) {
        p=float3(Tile.x+uv.x*Tile.z,-.015,Tile.y+uv.y*Tile.z);
        float trail=1-smoothstep(1.3,2.3,abs(p.x-path(p.z)));
        color=lerp(float3(.075,.105,.035),float3(.24,.185,.11),trail);
        color*=.85+.15*hash(floor(p.xz*7));
    } else {
        // Low-poly tree placeholders: opaque geometry avoids foliage overdraw.
        float ring=floor(vertex/6), angle=(ring+uv.x)*6.283185/8;
        float height=7+c*5;
        root.x+= (root.x>path(root.y)?1:-1)*3;
        float radius=Tile.w<2.5 ? .19+c*.16 : (1-uv.y)*2.7+.12;
        float y=Tile.w<2.5 ? uv.y*height : height*.52+uv.y*height*.65;
        p=float3(root.x+cos(angle)*radius,y,root.y+sin(angle)*radius);
        if(Tile.w>2.5) p.x+=sin(EyeTime.w+root.y)*.10*uv.y*Options.x;
        n=normalize(float3(cos(angle),Tile.w<2.5?0:.5,sin(angle)));
        color=Tile.w<2.5?float3(.17,.105,.055):float3(.055,.15,.065)*(.8+c*.4);
    }
    o.world=p; o.normal=n; o.color=color;
    o.position=mul(float4(p,1),ViewProjection); return o;
}
