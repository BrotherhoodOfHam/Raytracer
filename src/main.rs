mod surface;
mod camera;
mod loader;
mod shaders;

use {
    camera::Camera,
    loader::Vk,
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
        buffer::{ BufferUsage, CpuBufferPool, ImmutableBuffer },
        command_buffer::AutoCommandBufferBuilder,
        pipeline::{ ComputePipeline },
        descriptor::{
            PipelineLayoutAbstract,
            descriptor_set::{
                FixedSizeDescriptorSetsPool
            }
        }
    },
    cgmath::{
        Matrix4, Vector3, Vector4,
        vec3, vec4
    }
};

#[derive(Clone)]
struct Globals
{
    camera_to_world: Matrix4<f32>,
    time:   f32
}

#[derive(Clone)]
struct Sphere
{
    center:   Vector3<f32>,
    radius:   f32,
    material: i32,
    padding:  [i32;3]
}

impl Sphere
{
    fn new(center: Vector3<f32>, radius: f32, material: i32) -> Self
    {
        Sphere { center, radius, material, padding: [0,0,0] }
    }
}

#[derive(Clone)]
struct Material
{
    diffuse_colour: Vector4<f32>
}

impl Material
{
    fn new(diffuse_colour: Vector4<f32>) -> Self
    {
        Self { diffuse_colour }
    }
}

fn main()
{
    let ev = EventLoop::new();
    let window = WindowBuilder::new().build(&ev).unwrap();
    let mut vk = Vk::new(window);
    
    let cs = shaders::cs::Shader::load(vk.device.clone()).expect("failed to load shader");

    let compute_pipeline = Arc::new(
        ComputePipeline::new(vk.device.clone(), &cs.main_entry_point(), &()).unwrap()
    );
    let layout = compute_pipeline.layout().descriptor_set_layout(0).unwrap().clone();

    let scene = [
        Sphere::new(vec3( 4.0, -1.5, 2.0), 1.5, 0),
        Sphere::new(vec3( 0.0, -0.5, 2.0), 0.5, 1),
        Sphere::new(vec3(-3.0, -1.0, 2.0), 1.0, 2)
    ];

    let materials = [
        Material::new(vec4(1.0, 0.2, 0.1, 1.0)),
        Material::new(vec4(0.5, 1.0, 0.2, 1.0)),
        Material::new(vec4(0.1, 0.1, 1.0, 1.0))
    ];

    let (scene, materials) = {
        let (scene, _) = ImmutableBuffer::from_data(scene, BufferUsage::uniform_buffer(), vk.queue.clone()).unwrap();
        let (materials, _) = ImmutableBuffer::from_data(materials, BufferUsage::uniform_buffer(), vk.queue.clone()).unwrap();
        (scene, materials)
    };

    let globals_pool = CpuBufferPool::new(vk.device.clone(), BufferUsage::uniform_buffer());
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
                let globals = globals_pool.next(Globals {
                    camera_to_world: camera.transform(),
                    time
                }).unwrap();

                // descriptor set
                let set = descriptor_pool.next()
                    .add_image(frame.image.clone()).unwrap()
                    .add_buffer(globals.clone()).unwrap()
                    .add_buffer(scene.clone()).unwrap()
                    .add_buffer(materials.clone()).unwrap()
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
