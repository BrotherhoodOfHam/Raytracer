use {
    cgmath::{
        prelude::*,
        Matrix4, Matrix3, Vector3, Deg
    },
    winit::event::{
        KeyboardInput,
        ElementState,
        VirtualKeyCode as Key
    }
};

pub struct Camera
{
    pos: Vector3<f32>,
    angle_v:  f32,
    angle_h:  f32,
    
    actions:  u16,
    boost:    bool,
}

// actions
const FORWARD:    u16 = 1 << 0;
const BACKWARD:   u16 = 1 << 1;
const LEFT:       u16 = 1 << 2;
const RIGHT:      u16 = 1 << 3;
const UP:         u16 = 1 << 4;
const DOWN:       u16 = 1 << 5;
const LOOK_LEFT:  u16 = 1 << 6;
const LOOK_RIGHT: u16 = 1 << 7;
const LOOK_UP:    u16 = 1 << 8;
const LOOK_DOWN:  u16 = 1 << 9;

impl Camera
{
    pub fn new() -> Self
    {
        Self {
            pos:     Vector3::new(0.0, 1.0, 0.0),
            boost:   false,
            angle_h: 0.0,
            angle_v: 0.0,
            actions: 0
        }
    }

    pub fn transform(&self) -> Matrix4<f32>
    {
        let h = Matrix4::from_angle_y(Deg(self.angle_h)); // horizontal
        let v = Matrix4::from_angle_x(Deg(self.angle_v)); // vertical
        let t = Matrix4::from_translation(Vector3::new(self.pos.x, -self.pos.y, self.pos.z));
        // set pitch, then yaw, then translate (right to left)
        t * (h * v)
    }

    pub fn update(&mut self, delta: f32)
    {
        let acts = self.actions;
        if acts != 0
        {
            let deg = 60.0 * delta;

            if acts & LOOK_UP != 0    { self.angle_v += deg; }
            if acts & LOOK_DOWN != 0  { self.angle_v -= deg; }
            if acts & LOOK_RIGHT != 0 { self.angle_h += deg; }
            if acts & LOOK_LEFT != 0  { self.angle_h -= deg; }

            self.angle_h = self.angle_h % 360.0;
            self.angle_v = if self.angle_v < -90.0 { -90.0 } else if self.angle_v > 90.0 { 90.0 } else { self.angle_v };

            let mut offset = Vector3::new(0.0, 0.0, 0.0);
            if acts & FORWARD != 0  { offset.z += 1.0 }
            if acts & BACKWARD != 0 { offset.z -= 1.0 }
            if acts & RIGHT != 0    { offset.x += 1.0 }
            if acts & LEFT != 0     { offset.x -= 1.0 }
            if acts & UP != 0       { offset.y += 1.0 }
            if acts & DOWN != 0     { offset.y -= 1.0 }

            if !offset.is_zero()
            {
                let rot = Matrix3::from_angle_y(Deg(self.angle_h));

                let speed = if self.boost { 5.0 * 4.0 } else { 5.0 };
                self.pos += (rot * offset).normalize() * (speed * delta as f32);
            }
        }
    }

    pub fn handle_input(&mut self, input: &KeyboardInput)
    {
        if let Some(keycode) = input.virtual_keycode
        {
            let pressed = input.state == ElementState::Pressed;
            match keycode
            {
                Key::W => if pressed { self.actions |= FORWARD  } else { self.actions &= !FORWARD },
                Key::S => if pressed { self.actions |= BACKWARD } else { self.actions &= !BACKWARD },
                Key::A => if pressed { self.actions |= LEFT     } else { self.actions &= !LEFT },
                Key::D => if pressed { self.actions |= RIGHT    } else { self.actions &= !RIGHT },
                Key::Q => if pressed { self.actions |= UP       } else { self.actions &= !UP },
                Key::Z => if pressed { self.actions |= DOWN     } else { self.actions &= !DOWN },
                Key::Left  => if pressed { self.actions |= LOOK_LEFT  } else { self.actions &= !LOOK_LEFT },
                Key::Right => if pressed { self.actions |= LOOK_RIGHT } else { self.actions &= !LOOK_RIGHT },
                Key::Up    => if pressed { self.actions |= LOOK_UP    } else { self.actions &= !LOOK_UP },
                Key::Down  => if pressed { self.actions |= LOOK_DOWN  } else { self.actions &= !LOOK_DOWN },
                Key::LShift => self.boost = pressed,
                _ => {}
            }
        }
    }
}
