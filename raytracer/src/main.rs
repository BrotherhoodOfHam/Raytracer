
mod surface;

use {
    surface::{ VkSurfaceBuild, required_extensions },
    std::{
        sync::Arc,
        iter::Iterator,
        cell::Cell
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
        buffer::{BufferUsage, ImmutableBuffer},
        image::{ ImageUsage, SwapchainImage, StorageImage, Dimensions },
        framebuffer::{Framebuffer, RenderPassAbstract, FramebufferAbstract, RenderPass, SubpassContents },
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
    }
};

mod cs
{
    vulkano_shaders::shader! {
        ty: "compute",
        path: "src/trace.comp.glsl"
    }
}

fn create_descriptor_sets<W>(images: Vec<Arc<SwapchainImage<W>>>, layout: Arc<UnsafeDescriptorSetLayout>) ->
    Vec<Arc<impl DescriptorSet>>
{
    images.iter().map(|image| {
        Arc::new(PersistentDescriptorSet::start(layout.clone())
            .add_image(image.clone()).unwrap()
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
    let dim = surface.window().inner_size();

    let (mut swapchain, swapchain_images) = Swapchain::new(
        device.clone(), surface.clone(),
        caps.min_image_count, format,
        dim.into(), 1, ImageUsage { color_attachment: true, storage: true, ..ImageUsage::none() }, //ImageUsage::color_attachment(),
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

    let mut rebuild_swapchain = false;
    let mut swapchain_descriptor_sets = create_descriptor_sets(swapchain_images, layout.clone());
    let mut end_of_previous_frame = Some(sync::now(device.clone()).boxed());

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

                let mut future = end_of_previous_frame.take().unwrap();
                future.cleanup_finished();

                if rebuild_swapchain
                {
                    println!("{:?}", surface.window().inner_size());
                    rebuild_swapchain = false;

                    let (s, images) = swapchain.recreate_with_dimensions(surface.window().inner_size().into()).expect("failed to resize swapchain");
                    swapchain = s;

                    swapchain_descriptor_sets = create_descriptor_sets(images, layout.clone());
                }

                // next frame
                let (idx, _, acquire_future) = vulkano::swapchain::acquire_next_image(swapchain.clone(), None).unwrap();

                // build command buffer
                let cmd = {

                    let mut cmd = AutoCommandBufferBuilder::primary_one_time_submit(device.clone(), queue.family()).unwrap();
    
                    let dim = surface.window().inner_size();
                    cmd.dispatch([dim.width / 8, dim.height / 8, 1], compute_pipeline.clone(), swapchain_descriptor_sets[idx].clone(), ()).unwrap();
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
