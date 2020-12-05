mod surface;
mod camera;
mod vk;

use {
    camera::Camera,
    vk::Vk,
    std::{
        sync::Arc,
        time::SystemTime
    },
    winit::{
        event_loop::{ EventLoop, ControlFlow },
        event::{ Event, WindowEvent },
        window::{ WindowBuilder }
    },
    vulkano::{
        buffer::{ BufferUsage, CpuBufferPool },
        command_buffer::AutoCommandBufferBuilder,
        pipeline::{ ComputePipeline },
        descriptor::{
            PipelineLayoutAbstract,
            descriptor_set::{
                FixedSizeDescriptorSetsPool
            }
        }
    },
    cgmath::{ Matrix4 }
};

mod cs
{
    vulkano_shaders::shader! {
        ty: "compute",
        path: "src/trace.comp.glsl"
    }
}

#[derive(Clone)]
struct Uniforms
{
    camera_to_world: Matrix4<f32>,
    time:   f32
}

fn main()
{
    let ev = EventLoop::new();
    let window = WindowBuilder::new().build(&ev).unwrap();
    let mut vk = Vk::new(window);
    
    let cs = cs::Shader::load(vk.device.clone()).expect("failed to load shader");

    let compute_pipeline = Arc::new(ComputePipeline::new(vk.device.clone(), &cs.main_entry_point(), &()).unwrap());
    let layout = compute_pipeline.layout().descriptor_set_layout(0).unwrap().clone();

    let uniform_buffers: CpuBufferPool<Uniforms> = CpuBufferPool::new(vk.device.clone(), BufferUsage::uniform_buffer());
    let mut descriptor_pool = FixedSizeDescriptorSetsPool::new(layout.clone());

    // state
    let mut camera = Camera::new();
    let mut time = 0.0;

    let mut start = SystemTime::now();
    
    // event loop
    ev.run(move |event, _, ctrl| {
        match event
        {
            Event::WindowEvent { event, .. } => match event {
                WindowEvent::CloseRequested => {
                    *ctrl = ControlFlow::Exit;
                },
                WindowEvent::Resized(_) => {
                    vk.mark_for_resize();
                },
                WindowEvent::KeyboardInput { input, .. } => {
                    camera.handle_input(&input);
                },
                _ => {}
            },
            Event::RedrawEventsCleared => {

                let now = SystemTime::now();
                let delta = now.duration_since(start).unwrap();
                let delta = delta.as_secs() as f64 + delta.subsec_nanos() as f64 * 1e-9;
                start = now;
                
                // update state
                time += delta as f32;
                camera.update(delta as f32);

                let frame = vk.next_frame();

                // update uniform buffer
                let buffer = uniform_buffers.next(Uniforms {
                    camera_to_world: camera.transform(),
                    time
                }).unwrap();

                // descriptor set
                let set = descriptor_pool.next()
                    .add_image(frame.image.clone()).unwrap()
                    .add_buffer(buffer.clone()).unwrap()
                    .build().unwrap();

                let [width, height] = frame.image.dimensions();

                // build command buffer
                let cmd = {
                    let mut cmd = AutoCommandBufferBuilder::primary_one_time_submit(vk.device.clone(), vk.queue.family()).unwrap();
                    cmd.dispatch([width / 8, height / 8, 1], compute_pipeline.clone(), set, ()).unwrap();
                    cmd.build()
                }.unwrap();

                vk.submit(frame, cmd);
            },
            _ => {}
        }
    })
}
