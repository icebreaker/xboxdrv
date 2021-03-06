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

#include "axis_filter.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <iostream>
#include <math.h>
#include <sstream>
#include <stdexcept>

#include "helper.hpp"

namespace {

/** converts the arbitary range to [-1,1] */
inline float to_float(int value, int min, int max)
{
  return static_cast<float>(value - min) / static_cast<float>(max - min) * 2.0f - 1.0f;
}

/** converts the range [-1,1] to [min,max] */
inline int from_float(float value, int min, int max)
{
  return (value + 1.0f) / 2.0f * static_cast<float>(max - min) + min;
}

} // namespace

AxisFilterPtr
AxisFilter::from_string(const std::string& str)
{
  std::string::size_type p = str.find(':');
  const std::string& filtername = str.substr(0, p);
  std::string rest;

  if (p != std::string::npos) 
    rest = str.substr(p+1);

  if (filtername == "invert")
  {
    return AxisFilterPtr(new InvertAxisFilter);
  }
  else if (filtername == "calibration" || filtername == "cal")
  {
    return AxisFilterPtr(CalibrationAxisFilter::from_string(rest));
  }
  else if (filtername == "sensitivity" || filtername == "sen")
  {
    return AxisFilterPtr(SensitivityAxisFilter::from_string(rest));
  }
  else if (filtername == "deadzone" || filtername == "dead")
  {
    return AxisFilterPtr(DeadzoneAxisFilter::from_string(rest));
  }
  else if (filtername == "relative" || filtername == "rel")
  {
    return AxisFilterPtr(RelativeAxisFilter::from_string(rest));
  }
  else
  {
    std::ostringstream out;
    out << "unknown AxisFilter '" << filtername << "'";
    throw std::runtime_error(out.str());
  }
}

int
InvertAxisFilter::filter(int value, int min, int max)
{
  // FIXME: take min/max into account
  return -value;
}

SensitivityAxisFilter*
SensitivityAxisFilter::from_string(const std::string& str)
{
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  tokenizer tokens(str, boost::char_separator<char>(":", "", boost::keep_empty_tokens));
  
  float sensitivity = 0.0f;

  int j = 0;
  for(tokenizer::iterator i = tokens.begin(); i != tokens.end(); ++i, ++j)
  {
    switch(j)
    {
      case 0: sensitivity = boost::lexical_cast<float>(*i); break;
      default: throw std::runtime_error("to many arguments");
    };
  }

  return new SensitivityAxisFilter(sensitivity);
}

SensitivityAxisFilter::SensitivityAxisFilter(float sensitivity) :
  m_sensitivity(sensitivity)
{  
}

int
SensitivityAxisFilter::filter(int value, int min, int max)
{
  float pos = to_float(value, min, max);

  float t = powf(2, m_sensitivity);

  if (pos > 0)
  {
    pos = powf(1.0f - powf(1.0f - pos, t), 1 / t);
    return from_float(pos, min, max);
  }
  else
  {
    pos = powf(1.0f - powf(1.0f - -pos, t), 1 / t);
    return from_float(-pos, min, max);
  }
}

CalibrationAxisFilter*
CalibrationAxisFilter::from_string(const std::string& str)
{
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  tokenizer tokens(str, boost::char_separator<char>(":", "", boost::keep_empty_tokens));

  int min    = 0;
  int center = 0;
  int max    = 0;

  int j = 0;
  for(tokenizer::iterator i = tokens.begin(); i != tokens.end(); ++i, ++j)
  {
    switch(j)
    {
      case 0: min    = boost::lexical_cast<int>(*i); break;
      case 1: center = boost::lexical_cast<int>(*i); break;
      case 2: max    = boost::lexical_cast<int>(*i); break;
      default: throw std::runtime_error("to many arguments");
    };
  }

  return new CalibrationAxisFilter(min, center, max);
}

CalibrationAxisFilter::CalibrationAxisFilter(int min, int center, int max) :
  m_min(min),
  m_center(center),
  m_max(max)
{
}

int
CalibrationAxisFilter::filter(int value, int min, int max)
{
  if (value < m_center)
    value = -min * (value - m_center) / (m_center - m_min);
  else if (value > m_center)
    value = max * (value - m_center) / (m_max - m_center);
  else
    value = 0;

  return Math::clamp(min, value, max);
}

DeadzoneAxisFilter*
DeadzoneAxisFilter::from_string(const std::string& str)
{
  int  deadzone = 0;
  bool smooth   = true;

  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  tokenizer tokens(str, boost::char_separator<char>(":", "", boost::keep_empty_tokens));
  int idx = 0;
  for(tokenizer::iterator t = tokens.begin(); t != tokens.end(); ++t, ++idx)
  {
    switch(idx)
    {
      case 0: deadzone = boost::lexical_cast<int>(*t); break;
      case 1: smooth   = boost::lexical_cast<bool>(*t); break;
      default: throw std::runtime_error("to many arguments"); break;
    }
  }

  return new DeadzoneAxisFilter(deadzone, smooth);
}

DeadzoneAxisFilter::DeadzoneAxisFilter(int deadzone, bool smooth) :
  m_deadzone(deadzone),
  m_smooth(smooth)
{
}

int
DeadzoneAxisFilter::filter(int value, int min, int max)
{
  if (/*FIXME !m_smooth */ true)
  {
    if (abs(value) < m_deadzone)
    {
      return 0;
    }
    else
    {
      return value;
    }
  }
  else // (m_smooth)
  {
    // FIXME: not implemented
    assert(!"not implemented");
    /*
      if (value < -deadzone) 
      {
      const float scale = 32768 / (32768 - deadzone);
      rv += deadzone;
      rv *= scale;
      rv -= 0.5;
      }
      else if (value > deadzone) 
      {
      const float scale = 32767 / (32767 - deadzone);
      rv -= deadzone;
      rv *= scale;
      rv += 0.5;
      } 
      else 
      {
      return 0;
      }
    */
  }
}

RelativeAxisFilter*
RelativeAxisFilter::from_string(const std::string& str)
{
  int speed = 20000;

  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  tokenizer tokens(str, boost::char_separator<char>(":", "", boost::keep_empty_tokens));
  int idx = 0;
  for(tokenizer::iterator t = tokens.begin(); t != tokens.end(); ++t, ++idx)
  {
    switch(idx)
    {
      case 0: speed = boost::lexical_cast<int>(*t); break;
      default: throw std::runtime_error("to many arguments"); break;
    }
  }

  return new RelativeAxisFilter(speed);
  
}

RelativeAxisFilter::RelativeAxisFilter(int speed) :
  m_speed(speed),
  m_value(0),
  m_state(0),
  m_min(-1),
  m_max(1)
{
}

void
RelativeAxisFilter::update(int msec_delta)
{
  m_state += m_speed * m_value / m_max * msec_delta / 1000;
  
  m_state = Math::clamp(m_min, m_state, m_max);
}

int
RelativeAxisFilter::filter(int value, int min, int max)
{
  m_value = value;

  m_min   = min;
  m_max   = max;

  return m_state;
}

/* EOF */
