use {
    crate::surface::{ required_extensions, create_vk_surface },
    std::{
        sync::Arc,
        iter::Iterator,
    },
    winit::{
        window::Window
    },
    vulkano::{
        instance::{ Instance, PhysicalDevice },
        device::{ Device, DeviceExtensions, Queue },
        swapchain::{ SwapchainAcquireFuture, Swapchain, Surface, PresentMode, FullscreenExclusive, ColorSpace, SurfaceTransform },
        image::{ ImageUsage, SwapchainImage },
        command_buffer::CommandBuffer,
        sync::{ GpuFuture },
        sync
    }
};

pub struct Vk
{
    #[allow(dead_code)]
    instance:          Arc<Instance>,
    
    pub device:        Arc<Device>,
    pub queue:         Arc<Queue>,
    pub surface:       Arc<Surface<Window>>,
    swapchain:         Arc<Swapchain<Window>>,
    swapchain_images:  Vec<Arc<SwapchainImage<Window>>>,

    rebuild_swapchain: bool,
    end_of_frame:      Option<Box<dyn GpuFuture>>
}

pub struct Frame
{
    pub idx:        usize,
    pub image:      Arc<SwapchainImage<Window>>,
    acquire_future: SwapchainAcquireFuture<Window>
}

impl Vk
{
    pub fn new(window: Window) -> Self
    {
        let instance = Instance::new(None, &required_extensions(), None)
            .expect("failed to create instance");
    
        let physical = PhysicalDevice::enumerate(&instance).next()
            .expect("no available devices");
    
        println!("using device: {} (type: {:?})", physical.name(), physical.ty());
    
        let surface = create_vk_surface(window, instance.clone()).unwrap();
    
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
    
        std::assert!(caps.supported_usage_flags.storage);

        let (swapchain, swapchain_images) = Swapchain::new(
            device.clone(), surface.clone(),
            caps.min_image_count, format,
            frame_dim.into(), 1, ImageUsage { color_attachment: true, storage: true, ..ImageUsage::none() }, //ImageUsage::color_attachment(),
            &queue.clone(),
            SurfaceTransform::Identity, alpha,
            PresentMode::Fifo, FullscreenExclusive::Default,
            true, ColorSpace::SrgbNonLinear
        ).unwrap();

        Vk {
            rebuild_swapchain: false,
            end_of_frame: Some(sync::now(device.clone()).boxed()),
            instance, device, queue, surface, swapchain, swapchain_images
        }
    }

    pub fn mark_for_resize(&mut self)
    {
        self.rebuild_swapchain = true;
    }

    pub fn next_frame(&mut self) -> Frame
    {
        let mut future = self.end_of_frame.take().unwrap();
        future.cleanup_finished();
        self.end_of_frame = Some(future);
        
        let frame_dim = self.surface.window().inner_size();

        if self.rebuild_swapchain
        {
            self.rebuild_swapchain = false;

            let (s, i) = self.swapchain.recreate_with_dimensions(frame_dim.into()).expect("failed to resize swapchain");
            self.swapchain = s;
            self.swapchain_images = i;

            println!("{:?}", frame_dim);
        }

        // next frame
        let (idx, _, acquire_future) = match vulkano::swapchain::acquire_next_image(self.swapchain.clone(), None) {
            Ok(r) => r,
            Err(vulkano::swapchain::AcquireError::OutOfDate) => {
                self.rebuild_swapchain = true;
                return self.next_frame();
            },
            Err(err) => panic!("{:?}", err)
        };

        Frame {
            idx,
            image:   self.swapchain_images[idx].clone(),
            acquire_future
        }
    }

    pub fn submit<Cb: CommandBuffer + 'static>(&mut self, frame: Frame, cmd: Cb)
    {
        let future = self.end_of_frame.take().unwrap()
            .join(frame.acquire_future)
            .then_execute(self.queue.clone(), cmd).expect("failed to execute command buffer")
            .then_swapchain_present(self.queue.clone(), self.swapchain.clone(), frame.idx)
            .then_signal_fence_and_flush();

        self.end_of_frame = Some(match future {
            Ok(future) => {
                future.boxed()
            },
            Err(sync::FlushError::OutOfDate) => {
                self.rebuild_swapchain = true;
                sync::now(self.device.clone()).boxed()
            },
            Err(e) => {
                println!("{:?}", e);
                sync::now(self.device.clone()).boxed()
            }
        });
    }
}
