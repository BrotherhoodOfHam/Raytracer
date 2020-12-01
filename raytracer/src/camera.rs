
use {
    cgmath::{
        prelude::*,
        Matrix4, 
        Vector3
    },
    winit::event::{
        KeyboardInput,
        ElementState,
        VirtualKeyCode as Key
    }
};

pub struct Camera
{
    position: Vector3<f32>,

    speed: f32,
    actions: u8,
    boost: bool,
}

impl Camera
{
    pub fn new() -> Self
    {
        Self {
            position: Vector3::new(0.0, 1.0, 0.0),
            speed:    5.0,
            boost:    false,
            actions:  0
        }
    }

    pub fn transform(&self) -> Matrix4<f32>
    {
        Matrix4::from_translation(self.position).invert().unwrap()
    }

    pub fn move_by(&mut self, offset: Vector3<f32>)
    {
        self.position += offset;
    }
    
    const FORWARD: u8  = 1 << 0;
    const BACKWARD: u8 = 1 << 1;
    const LEFT: u8     = 1 << 2;
    const RIGHT: u8    = 1 << 3;
    const UP: u8       = 1 << 4;
    const DOWN: u8     = 1 << 5;

    pub fn handle_input(&mut self, input: &KeyboardInput)
    {
        if let Some(keycode) = input.virtual_keycode
        {
            let pressed = input.state == ElementState::Pressed;
            match keycode
            {
                Key::W => if pressed { self.actions |= Camera::FORWARD }  else { self.actions &= !Camera::FORWARD },
                Key::S => if pressed { self.actions |= Camera::BACKWARD } else { self.actions &= !Camera::BACKWARD },
                Key::A => if pressed { self.actions |= Camera::LEFT }     else { self.actions &= !Camera::LEFT },
                Key::D => if pressed { self.actions |= Camera::RIGHT }    else { self.actions &= !Camera::RIGHT },
                Key::Q => if pressed { self.actions |= Camera::UP }       else { self.actions &= !Camera::UP },
                Key::Z => if pressed { self.actions |= Camera::DOWN }     else { self.actions &= !Camera::DOWN },
                Key::LShift => self.boost = pressed,
                _ => {}
            }
        }
    }

    pub fn update(&mut self, delta: f64)
    {
        if self.actions != 0
        {
            let mut move_dir = Vector3::new(0.0, 0.0, 0.0);
            if self.actions & Camera::FORWARD != 0  { move_dir.z = -1.0 }
            if self.actions & Camera::BACKWARD != 0 { move_dir.z =  1.0 }
            if self.actions & Camera::RIGHT != 0    { move_dir.x = -1.0 }
            if self.actions & Camera::LEFT != 0     { move_dir.x =  1.0 }
            if self.actions & Camera::DOWN != 0     { move_dir.y = -1.0 }
            if self.actions & Camera::UP != 0       { move_dir.y =  1.0 }

            let speed = if self.boost { self.speed * 4.0 } else { self.speed };
            self.move_by(move_dir.normalize() * speed * delta as f32);
        }
    }
}
