mod surface;
mod camera;

use {
    camera::Camera,
    surface::{ VkSurfaceBuild, required_extensions },
    std::{
        sync::Arc,
        iter::Iterator,
        time::SystemTime
    },
    winit::{
        event_loop::{ EventLoop, ControlFlow },
        event::{ Event, WindowEvent },
        window::{ WindowBuilder }
    },
    vulkano::{
        instance::{ Instance, PhysicalDevice },
        device::{ Device, DeviceExtensions },
        swapchain::{ Swapchain, PresentMode, FullscreenExclusive, ColorSpace, SurfaceTransform },
        buffer::{ BufferUsage, CpuBufferPool },
        image::{ ImageUsage },
        sync,
        sync::{ GpuFuture },
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
        Matrix4
    }
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
    camera: Matrix4<f32>,
    time:   f32
}

fn main()
{
    let instance = Instance::new(None, &required_extensions(), None)
        .expect("failed to create instance");

    let physical = PhysicalDevice::enumerate(&instance).next()
        .expect("no available devices");

    println!("using device: {} (type: {:?})", physical.name(), physical.ty());

    let ev = EventLoop::new();
    let surface = WindowBuilder::new()
        .build_vk_surface(&ev, instance.clone())
        .unwrap();

    let queue_family = physical.queue_families()
        .find(|&q| q.supports_graphics() && surface.is_supported(q).unwrap_or(false))
        .expect("no available graphics queue family");

    let (device, mut queues) = Device::new(
            physical, 
            physical.supported_features(), 
            &DeviceExtensions { khr_swapchain: true, ..DeviceExtensions::none() }, 
            [(queue_family, 0.5)].iter().cloned()
        ).expect("failed to create device and queues");

    let queue = queues.next().unwrap();

    let caps = surface.capabilities(physical).unwrap();
    let (format, _) = caps.supported_formats[0];
    let alpha = caps.supported_composite_alpha.iter().next().unwrap();
    let frame_dim = surface.window().inner_size();

    let (mut swapchain, mut swapchain_images) = Swapchain::new(
        device.clone(), surface.clone(),
        caps.min_image_count, format,
        frame_dim.into(), 1, ImageUsage { color_attachment: true, storage: true, ..ImageUsage::none() }, //ImageUsage::color_attachment(),
        &queue.clone(),
        SurfaceTransform::Identity, alpha,
        PresentMode::Fifo, FullscreenExclusive::Default,
        true, ColorSpace::SrgbNonLinear
    ).unwrap();


    std::assert!(caps.supported_usage_flags.storage);
    
    let cs = cs::Shader::load(device.clone()).expect("failed to load shader");

    let compute_pipeline = Arc::new(ComputePipeline::new(device.clone(), &cs.main_entry_point(), &()).unwrap());
    let layout = compute_pipeline.layout().descriptor_set_layout(0).unwrap().clone();

    let uniform_buffers: CpuBufferPool<Uniforms> = CpuBufferPool::new(device.clone(), BufferUsage::uniform_buffer());
    let mut descriptor_pool = FixedSizeDescriptorSetsPool::new(layout.clone());

    // state
    let mut camera = Camera::new();
    let mut time = 0.0;


    let mut rebuild_swapchain = false;
    let mut end_of_previous_frame = Some(sync::now(device.clone()).boxed());
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
                    rebuild_swapchain = true;
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

                let mut future = end_of_previous_frame.take().unwrap();
                future.cleanup_finished();
                
                let frame_dim = surface.window().inner_size();

                if rebuild_swapchain
                {
                    println!("{:?}", frame_dim);
                    rebuild_swapchain = false;

                    let (s, i) = swapchain.recreate_with_dimensions(frame_dim.into()).expect("failed to resize swapchain");
                    swapchain = s;
                    swapchain_images = i;
                }

                // next frame
                let (idx, _, acquire_future) = match vulkano::swapchain::acquire_next_image(swapchain.clone(), None) {
                    Ok(r) => r,
                    Err(vulkano::swapchain::AcquireError::OutOfDate) => {
                        rebuild_swapchain = true;
                        return;
                    },
                    Err(err) => panic!("{:?}", err)
                };

                // update state
                time += delta as f32;
                camera.update(delta);

                // update uniform buffer
                let buffer = uniform_buffers.next(Uniforms {
                    camera: camera.transform(),
                    time
                }).unwrap();

                // descriptor set
                let set = descriptor_pool.next()
                    .add_image(swapchain_images[idx].clone()).unwrap()
                    .add_buffer(buffer.clone()).unwrap()
                    .build().unwrap();

                // build command buffer
                let cmd = {
                    let mut cmd = AutoCommandBufferBuilder::primary_one_time_submit(device.clone(), queue.family()).unwrap();
                    cmd.dispatch([frame_dim.width / 8, frame_dim.height / 8, 1], compute_pipeline.clone(), set, ()).unwrap();
                    cmd.build()
                }.unwrap();

                let future = future
                    .join(acquire_future)
                    .then_execute(queue.clone(), cmd).expect("failed to execute command buffer")
                    .then_swapchain_present(queue.clone(), swapchain.clone(), idx)
                    .then_signal_fence_and_flush();

                end_of_previous_frame = match future {
                    Ok(future) => {
                        Some(future.boxed())
                    },
                    Err(sync::FlushError::OutOfDate) => {
                        rebuild_swapchain = true;
                        Some(sync::now(device.clone()).boxed())
                    },
                    Err(e) => {
                        println!("{:?}", e);
                        Some(sync::now(device.clone()).boxed())
                    }
                };
            },
            _ => {}
        }
    })
}
