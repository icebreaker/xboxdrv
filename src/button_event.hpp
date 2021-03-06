/*
**  Xbox360 USB Gamepad Userspace Driver
**  Copyright (C) 2010 Ingo Ruhnke <grumbel@gmx.de>
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

#ifndef HEADER_XBOXDRV_BUTTON_EVENT_HPP
#define HEADER_XBOXDRV_BUTTON_EVENT_HPP

#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <string>
#include <vector>

#include "button_filter.hpp"
#include "uinput_deviceid.hpp"

class uInput;
class ButtonEvent;
class ButtonEventHandler;

typedef boost::shared_ptr<ButtonEvent> ButtonEventPtr;

class ButtonEvent
{
public:
  static ButtonEventPtr invalid();
  static ButtonEventPtr create(ButtonEventHandler* handler);
  static ButtonEventPtr create_key(int code);
  static ButtonEventPtr create_key();
  static ButtonEventPtr create_abs(int code);
  static ButtonEventPtr create_rel(int code);
  static ButtonEventPtr from_string(const std::string& str);

protected:
  ButtonEvent(ButtonEventHandler* handler);

public: 
  void init(uInput& uinput) const;
  void send(uInput& uinput, bool value);
  void update(uInput& uinput, int msec_delta);
  std::string str() const;

  void set_filters(const std::vector<ButtonFilterPtr>& filters);

private:
  bool m_last_send_state;
  bool m_last_raw_state;
  boost::scoped_ptr<ButtonEventHandler> m_handler;
  std::vector<ButtonFilterPtr> m_filters;
};

class ButtonEventHandler
{
public:
  virtual ~ButtonEventHandler() {}
  
  virtual void init(uInput& uinput) const =0;
  virtual void send(uInput& uinput, bool value) =0;
  virtual void update(uInput& uinput, int msec_delta) =0;
  virtual std::string str() const =0;
};

class KeyButtonEventHandler : public ButtonEventHandler
{
public:
  static KeyButtonEventHandler* from_string(const std::string& str);

public:
  KeyButtonEventHandler();
  KeyButtonEventHandler(int code);

  void init(uInput& uinput) const;
  void send(uInput& uinput, bool value);
  void update(uInput& uinput, int msec_delta);

  std::string str() const;
  
private:
  static const int MAX_MODIFIER = 4;

  bool m_state;
  // Array is terminated by !is_valid()
  UIEvent m_codes[MAX_MODIFIER+1];
  UIEvent m_secondary_codes[MAX_MODIFIER+1];
  int m_hold_threshold;
  int m_hold_counter;
};

class AbsButtonEventHandler : public ButtonEventHandler
{
public:
  static AbsButtonEventHandler* from_string(const std::string& str);

public:
  AbsButtonEventHandler(int code);

  void init(uInput& uinput) const;
  void send(uInput& uinput, bool value);
  void update(uInput& uinput, int msec_delta) {}

  std::string str() const;

private:
  UIEvent m_code;
  int m_value;
};

class RelButtonEventHandler : public ButtonEventHandler
{
public:
  static RelButtonEventHandler* from_string(const std::string& str);

public:
  RelButtonEventHandler(const UIEvent& code);

  void init(uInput& uinput) const;
  void send(uInput& uinput, bool value);
  void update(uInput& uinput, int msec_delta) {}

  std::string str() const;

private:
  UIEvent m_code;

  int  m_value;
  int  m_repeat;
};

#endif

/* EOF */
