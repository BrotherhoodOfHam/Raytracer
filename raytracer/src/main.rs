
mod surface;

use {
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
        buffer::{ BufferUsage, CpuAccessibleBuffer },
        image::{ ImageUsage, SwapchainImage },
        sync,
        sync::{ GpuFuture },
        command_buffer::AutoCommandBufferBuilder,
        pipeline::{ ComputePipeline },
        descriptor::{
            PipelineLayoutAbstract,
            descriptor_set::{
                DescriptorSet,
                PersistentDescriptorSet,
                UnsafeDescriptorSetLayout,
            }
        }
    },
    cgmath::{ Matrix4, Vector3, SquareMatrix }
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

fn build_descriptor_sets<W, A>(images: Vec<Arc<SwapchainImage<W>>>, buffer: Arc<CpuAccessibleBuffer<Uniforms, A>>, layout: Arc<UnsafeDescriptorSetLayout>) ->
    Vec<Arc<impl DescriptorSet>>
{
    images.iter().map(|image| {
        Arc::new(PersistentDescriptorSet::start(layout.clone())
            .add_image(image.clone()).unwrap()
            .add_buffer(buffer.clone()).unwrap()
            .build().unwrap()
        )
    }).collect()
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

    let (mut swapchain, swapchain_images) = Swapchain::new(
        device.clone(), surface.clone(),
        caps.min_image_count, format,
        frame_dim.into(), 1, ImageUsage { color_attachment: true, storage: true, ..ImageUsage::none() }, //ImageUsage::color_attachment(),
        &queue.clone(),
        SurfaceTransform::Identity, alpha,
        PresentMode::Fifo, FullscreenExclusive::Default,
        true, ColorSpace::SrgbNonLinear
    ).unwrap();
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::assert!(caps.supported_usage_flags.storage);
    
    let cs = cs::Shader::load(device.clone()).expect("failed to load shader");

    let compute_pipeline = Arc::new(ComputePipeline::new(device.clone(), &cs.main_entry_point(), &()).unwrap());
    let layout = compute_pipeline.layout().descriptor_set_layout(0).unwrap().clone();

    let mut uniforms = Uniforms {
        camera: Matrix4::from_translation(Vector3::new(0.0, 1.0, 0.0)).invert().unwrap(),
        time:   0.0
    };

    let uniform_buffer = CpuAccessibleBuffer::from_data(
        device.clone(), BufferUsage::uniform_buffer(), false, uniforms.clone()
    ).unwrap();

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    let mut rebuild_swapchain = false;
    let mut swapchain_descriptor_sets = build_descriptor_sets(swapchain_images, uniform_buffer.clone(), layout.clone());
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

                    let (s, images) = swapchain.recreate_with_dimensions(frame_dim.into()).expect("failed to resize swapchain");
                    swapchain = s;

                    swapchain_descriptor_sets = build_descriptor_sets(images, uniform_buffer.clone(), layout.clone());
                }

                // next frame
                let (idx, _, acquire_future) = vulkano::swapchain::acquire_next_image(swapchain.clone(), None).unwrap();

                // update state
                uniforms.time += delta as f32;

                {
                    let mut write = uniform_buffer.write().expect("gpu locked");
                    write.camera = uniforms.camera;
                    write.time = uniforms.time;
                }

                // build command buffer
                let cmd = {

                    let mut cmd = AutoCommandBufferBuilder::primary_one_time_submit(device.clone(), queue.family()).unwrap();
                    cmd.dispatch([frame_dim.width / 8, frame_dim.height / 8, 1], compute_pipeline.clone(), swapchain_descriptor_sets[idx].clone(), ()).unwrap();
                    cmd.build()

                }.unwrap();

                let future = future
                    .join(acquire_future)
                    .then_execute(queue.clone(), cmd).expect("failed to execute command buffer")
                    .then_swapchain_present(queue.clone(), swapchain.clone(), idx)
                    .then_signal_fence_and_flush().expect("failed to flush");

                end_of_previous_frame = Some(future.boxed());
            },
            _ => {}
        }
    })
}
