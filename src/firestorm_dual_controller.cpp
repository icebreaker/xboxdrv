/* 
**  Xbox/Xbox360 USB Gamepad Userspace Driver
**  Copyright (C) 2008 Ingo Ruhnke <grumbel@gmx.de>
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <errno.h>
#include <stdexcept>
#include <sstream>
#include <string.h>
#include <boost/format.hpp>

#include "helper.hpp"
#include "firestorm_dual_controller.hpp"
#include "usb_helper.hpp"

// 044f:b312
struct Firestorm_vsb_Msg
{
  unsigned int a   :1;
  unsigned int x   :1;
  unsigned int b   :1;
  unsigned int y   :1;

  unsigned int lb  :1;
  unsigned int lt  :1;
  unsigned int rb  :1;
  unsigned int rt  :1;

  unsigned int back  :1;
  unsigned int start :1;
  
  unsigned int thumb_l :1;
  unsigned int thumb_r :1;

  unsigned int dpad :4; // 0xf == center, 0x00 == up, clockwise + 1 each

  int x1 :8;
  int y1 :8;
  int x2 :8;
  unsigned int y2 :8;  
} __attribute__((__packed__));

// 044f:b304
struct FirestormMsg
{
  unsigned int a   :1;
  unsigned int x   :1;
  unsigned int b   :1;
  unsigned int y   :1;

  unsigned int lb  :1;
  unsigned int lt  :1;
  unsigned int rb  :1;
  unsigned int rt  :1;

  unsigned int back  :1;
  unsigned int start :1;
  
  unsigned int thumb_l :1;
  unsigned int thumb_r :1;

  unsigned int dummy :4;

  unsigned int dpad :8; // 0xf0 == center, 0x00 == up, clockwise + 10 each

  int x1 :8;
  int y1 :8;
  int x2 :8;
  unsigned int y2 :8;
} __attribute__((__packed__));

FirestormDualController::FirestormDualController(struct usb_device* dev_, bool is_vsb_, bool try_detach) :
  is_vsb(is_vsb_),
  dev(dev_),
  handle(),
  left_rumble(-1),
  right_rumble(-1)
{
  handle = usb_open(dev);
  if (!handle)
  {
    throw std::runtime_error("Error opening FirestormDualController");
  }
  else
  {
    int err = usb_claim_n_detach_interface(handle, 0, try_detach);
    if (err != 0) 
    {
      std::ostringstream out;
      out << "Error couldn't claim the USB interface: " << usb_strerror() << std::endl
          << "Try to run 'rmmod xpad' and then xboxdrv again or start xboxdrv with the option --detach-kernel-driver.";
      throw std::runtime_error(out.str());
    }
  }
}

FirestormDualController::~FirestormDualController()
{
  usb_release_interface(handle, 0); 
  usb_close(handle);
}

void
FirestormDualController::set_rumble(uint8_t left, uint8_t right)
{
  if (left_rumble  != left ||
      right_rumble != right)
  {
    left_rumble  = left;
    right_rumble = right;

    char cmd[] = { left, right, 0x00, 0x00 };
    if (is_vsb)
      usb_control_msg(handle, 0x21, 0x09, 0x0200, 0x00, cmd, sizeof(cmd), 0);
    else

      usb_control_msg(handle, 0x21, 0x09, 0x02, 0x00, cmd, sizeof(cmd), 0);
  }
}

void
FirestormDualController::set_led(uint8_t status)
{
  // not supported
}

bool
FirestormDualController::read(XboxGenericMsg& msg, bool verbose, int timeout)
{
  if (is_vsb)
    return read_vsb(msg, verbose, timeout);
  else
    return read_default(msg, verbose, timeout);
}

bool
FirestormDualController::read_vsb(XboxGenericMsg& msg, bool verbose, int timeout)
{
  Firestorm_vsb_Msg data;
  int ret = usb_interrupt_read(handle, 1 /*EndPoint*/, reinterpret_cast<char*>(&data), sizeof(data), timeout);

  if (ret == -ETIMEDOUT)
  {
    return false;
  }
  else if (ret < 0)
  { // Error
    std::ostringstream str;
    str << "USBError: " << ret << "\n" << usb_strerror();
    throw std::runtime_error(str.str());
  }
  else if (ret == sizeof(data))
  {
    if (0)
    { // debug output
      for(size_t i = 0; i < sizeof(data); ++i)
      {
        uint8_t v = reinterpret_cast<char*>(&data)[i];
        std::cout << boost::format("0x%02x ") % static_cast<int>(v);
      }
      std::cout << std::endl;
    }

    memset(&msg, 0, sizeof(msg));
    msg.type    = XBOX_MSG_XBOX360;

    msg.xbox360.a = data.a;
    msg.xbox360.b = data.b;
    msg.xbox360.x = data.x;
    msg.xbox360.y = data.y;

    msg.xbox360.lb = data.lb;
    msg.xbox360.rb = data.rb;

    msg.xbox360.lt = static_cast<unsigned char>(data.lt * 255);
    msg.xbox360.rt = static_cast<unsigned char>(data.rt * 255);

    msg.xbox360.start = data.start;
    msg.xbox360.back  = data.back;

    msg.xbox360.thumb_l = data.thumb_l;
    msg.xbox360.thumb_r = data.thumb_r;
      
    msg.xbox360.x1 = scale_8to16(data.x1);
    msg.xbox360.y1 = scale_8to16(data.y1);

    msg.xbox360.x2 = scale_8to16(data.x2);
    msg.xbox360.y2 = scale_8to16(data.y2 - 128);

    // Invert the axis
    msg.xbox360.y1 = negate_16(msg.xbox360.y1);
    msg.xbox360.y2 = negate_16(msg.xbox360.y2);

    // data.dpad == 0xf0 -> dpad centered
    // data.dpad == 0xe0 -> dpad-only mode is enabled

    if (data.dpad == 0x0 || data.dpad == 0x7 || data.dpad == 0x1)
      msg.xbox360.dpad_up   = 1;

    if (data.dpad == 0x1 || data.dpad == 0x2 || data.dpad == 0x3)
      msg.xbox360.dpad_right = 1;

    if (data.dpad == 0x3 || data.dpad == 0x4 || data.dpad == 0x5)
      msg.xbox360.dpad_down = 1;
      
    if (data.dpad == 0x5 || data.dpad == 0x6 || data.dpad == 0x7)
      msg.xbox360.dpad_left  = 1;

    return true;
  }
  else
  {
    return false;
  }  
}

bool
FirestormDualController::read_default(XboxGenericMsg& msg, bool verbose, int timeout)
{
  FirestormMsg data;
  int ret = usb_interrupt_read(handle, 1 /*EndPoint*/, reinterpret_cast<char*>(&data), sizeof(data), timeout);

  if (ret == -ETIMEDOUT)
  {
    return false;
  }
  else if (ret < 0)
  { // Error
    std::ostringstream str;
    str << "USBError: " << ret << "\n" << usb_strerror();
    throw std::runtime_error(str.str());
  }
  else if (ret == sizeof(data))
  {
    memset(&msg, 0, sizeof(msg));
    msg.type    = XBOX_MSG_XBOX360;

    msg.xbox360.a = data.a;
    msg.xbox360.b = data.b;
    msg.xbox360.x = data.x;
    msg.xbox360.y = data.y;

    msg.xbox360.lb = data.lb;
    msg.xbox360.rb = data.rb;

    msg.xbox360.lt = data.lt * 255;
    msg.xbox360.rt = data.rt * 255;

    msg.xbox360.start = data.start;
    msg.xbox360.back  = data.back;

    msg.xbox360.thumb_l = data.thumb_l;
    msg.xbox360.thumb_r = data.thumb_r;
      
    msg.xbox360.x1 = scale_8to16(data.x1);
    msg.xbox360.y1 = scale_8to16(data.y1);

    msg.xbox360.x2 = scale_8to16(data.x2);
    msg.xbox360.y2 = scale_8to16(data.y2 - 128);

    // Invert the axis
    msg.xbox360.y1 = negate_16(msg.xbox360.y1);
    msg.xbox360.y2 = negate_16(msg.xbox360.y2);

    // data.dpad == 0xf0 -> dpad centered
    // data.dpad == 0xe0 -> dpad-only mode is enabled

    if (data.dpad == 0x00 || data.dpad == 0x70 || data.dpad == 0x10)
      msg.xbox360.dpad_up   = 1;

    if (data.dpad == 0x10 || data.dpad == 0x20 || data.dpad == 0x30)
      msg.xbox360.dpad_right = 1;

    if (data.dpad == 0x30 || data.dpad == 0x40 || data.dpad == 0x50)
      msg.xbox360.dpad_down = 1;
      
    if (data.dpad == 0x50 || data.dpad == 0x60 || data.dpad == 0x70)
      msg.xbox360.dpad_left  = 1;

    return true;
  }
  else
  {
    return false;
  }
}

/* EOF */
