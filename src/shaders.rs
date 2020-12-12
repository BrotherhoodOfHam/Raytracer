/// Raytracing shader
pub mod cs
{
    vulkano_shaders::shader! {
        ty: "compute",
        path: "shaders/trace.comp.glsl"
    }
}
