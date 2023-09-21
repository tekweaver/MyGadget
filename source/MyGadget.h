#pragma once

#define DEBUG

#include <sys/reboot.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#ifndef DEBUG
/// @brief /sys/kernel/config/usb_gadget
static const std::filesystem::path g_ConfigFSPath =
    "/sys/kernel/config/usb_gadget";
/// @brief /sys/kernel/config/usb_gadget/MyGadget
static const std::filesystem::path g_GadgetPath =
    "/sys/kernel/config/usb_gadget/MyGadget";
/// @brief /boot/config.txt
static const std::filesystem::path g_ConfigTxt = "/boot/config.txt";
/// @brief /etc/modules
static const std::filesystem::path g_ModulesFile = "/etc/modules";
static const std::string g_UDCPath = "/sys/class/udc";
static void SYSTEM_REBOOT() {
  sync();
  reboot(RB_AUTOBOOT);
}
#else
/// @brief .sandbox
static const std::filesystem::path g_ConfigFSPath = ".sandbox";
/// @brief ./.sandbox/MyGadget
static const std::filesystem::path g_GadgetPath = ".sandbox/MyGadget";
/// @brief ./config.txt
static const std::filesystem::path g_ConfigTxt = "./config.txt";
/// @brief ./modules
static const std::filesystem::path g_ModulesFile = "./modules";
static void SYSTEM_REBOOT() { std::cout << "Poof...rebooted\n"; }

static const std::string g_UDCPath = ".sandbox/MyGadget";

#endif

static bool g_NeedsReboot = false;

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Type Defines

typedef std::vector<std::string>::iterator It;
typedef std::string String;
typedef std::vector<std::string> StringVec;
typedef std::ifstream ReadFile;
typedef std::ofstream WriteFile;
typedef std::map<std::string, std::string> ConfigMap;

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Class: MyGadget

/// @brief MyGadget Template
class MyGadget {
 private:
  virtual void Initialize(std::string deviceNum) = 0;
  static void initializeSystem();
  static void initializeConfigFS();
  /* data */
 protected:
 public:
  MyGadget();
  ~MyGadget();
};
MyGadget::MyGadget() {
  initializeSystem();
  initializeConfigFS();
}
void MyGadget::initializeSystem() {
  // check if system is not configured
  if (!std::filesystem::exists(g_ConfigFSPath)) {
    // config.txt
    {
      StringVec buffer;
      ReadFile file(g_ConfigTxt);
      String line;

      // Read file into buffer
      {
        if (file.good()) {
          while (std::getline(file, line)) buffer.push_back(line);
          file.close();
        }
      }
      // Modify buffer
      {
        {  // disable otg_mode if present
          for (It it = buffer.begin(); it != buffer.end(); ++it) {
            if (*it == "otg_mode=1") {
              g_NeedsReboot = true;
              *it = "#otg_mode=1";
            }
          }
        }
        {  // add DWC2 support
          It it = std::find(buffer.begin(), buffer.end(), "dtoverlay=dwc2");
          if (it == buffer.end()) {
            g_NeedsReboot = true;
            buffer.push_back("dtoverlay=dwc2");
          }
        }
      }
      // Write buffer to file
      {
        WriteFile file(g_ConfigTxt);
        if (file.good())
          for (std::string& s : buffer) file << s << std::endl;
        file.close();
      }
    }
    // modules
    {
      StringVec buffer;
      ReadFile file(g_ModulesFile);
      // Read module file to buffer
      {
        std::string line;
        if (file.good())
          while (std::getline(file, line)) buffer.push_back(line);
        file.close();
      }
      // Modify modules buffer
      {
        {
          It it = std::find(buffer.begin(), buffer.end(), "dwc2");
          if (it == buffer.end()) {
            g_NeedsReboot = true;
            buffer.push_back("dwc2");
          }
        }
        {
          It it = std::find(buffer.begin(), buffer.end(), "libcomposite");
          if (it == buffer.end()) {
            g_NeedsReboot = true;
            buffer.push_back("libcomposite");
          }
        }
      }
      // Write Buffer to File
      {
        WriteFile file(g_ModulesFile);
        if (file.good())
          for (std::string& s : buffer) file << s.c_str() << std::endl;
        file.close();
      }
    }
    // restart
    if (g_NeedsReboot == true) SYSTEM_REBOOT();
  }
}
void MyGadget::initializeConfigFS() {
  if (!std::filesystem::exists(g_GadgetPath)) {
    std::filesystem::path m_DeviceLocaleDir = g_GadgetPath / "strings/0x409";
    std::filesystem::path m_DeviceConfigDir = g_GadgetPath / "configs/c.1";
    std::filesystem::path m_DeviceConfigLocaleDir =
        g_GadgetPath / "configs/c.1/strings/0x409";

    WriteFile file;
    ConfigMap Config;
    /* Directories */
    {
      std::filesystem::create_directories(g_GadgetPath);
      std::filesystem::create_directories(m_DeviceLocaleDir);
      std::filesystem::create_directories(m_DeviceConfigLocaleDir);
    }
    /* Device Root Config */
    {
      Config["bcdDevice"] = "0x0100";
      Config["bcdUSB"] = "0x0200";
      Config["bDeviceClass"] = "0x00";
      Config["bDeviceProtocol"] = "0x00";
      Config["bDeviceSubClass"] = "0x00";
      Config["bMaxPacketSize0"] = "0x08";
      Config["idProduct"] = "0x0104";
      Config["idVendor"] = "0x1d6b";

      for (auto [name, value] : Config) {
        file.open(g_GadgetPath / name);
        file << value;
        file.close();
      }
      Config.clear();
    }
    /* Device Locale - English */
    {
      Config["manufacturer"] = "HID Gadget Factory";
      Config["product"] = "MyGadget";
      Config["serialnumber"] = "0524s21478435a6"; /* TODO */

      for (auto [name, value] : Config) {
        file.open(m_DeviceLocaleDir / name);
        file << value;
        file.close();
      }
      Config.clear();
    }
    /* Device Configuration */
    {
      Config["bmAttributes"] = "0x80";
      Config["MaxPower"] = "200";
      for (auto [name, value] : Config) {
        file.open(m_DeviceConfigDir / name);
        file << value;
        file.close();
      }
      Config.clear();
    }
    /* Default Configuration Locales */
    {
      file.open(m_DeviceConfigLocaleDir / "configuration");
      file << "Default Configurations";
      file.close();
    }
  }
}
MyGadget::~MyGadget() {
  std::vector<std::string> files;
  {
    for (auto& v : std::filesystem::directory_iterator(g_UDCPath))
      files.push_back(v.path().filename().c_str());
  }
  {
    WriteFile udc;
    udc.open(std::filesystem::path(g_GadgetPath / "UDC"));
    if (udc.good())
      for (auto& v : files) udc << v << std::endl;
    udc.close();
  }
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Class: Keyboard(MyGadget)

/// @brief Subclass of MyGadget.
class Keyboard : public MyGadget {
 private:
  void Initialize(std::string deviceNum) override;
  /* data */
 public:
  Keyboard(std::string deviceNum);
  ~Keyboard();
};
Keyboard::Keyboard(std::string deviceNum) { Initialize(deviceNum); }
void Keyboard::Initialize(std::string deviceNum) {
  std::string deviceName("hid.usb" + deviceNum);
  std::filesystem::path deviceDir(g_GadgetPath / "functions" / deviceName);
  if (!std::filesystem::exists(deviceDir)) {
    std::filesystem::create_directories(deviceDir);
    WriteFile file;
    ConfigMap Configs;
    {
      Configs["protocol"] = "1";
      Configs["report_length"] = "8";
      Configs["subclass"] = "1";
      Configs["report_desc"] =
          "05010906a101050719e029e715002501750195088102950"
          "1750881039505750"  // Keyboard
          "10508190129059102950175039103950675081500256505"
          "07190029658100c0";  // Descriptor

      for (auto [name, value] : Configs) {
        if (file.good()) {
          file.open(deviceDir / name);
          file << value;
          file.close();
        } else
          return;
      }
    }
    std::filesystem::create_directory_symlink(
        deviceDir,
        std::filesystem::path(g_GadgetPath / "configs/c.1/" / deviceName));
  }
}
Keyboard::~Keyboard() {}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Class: Joystick(MyGadget)

/// @brief Subclass of MyGadget.
class Joystick : public MyGadget {
 private:
  void Initialize(std::string deviceNum) override;
  /* data */
 public:
  Joystick(std::string deviceNum);
  ~Joystick();
};
Joystick::Joystick(std::string deviceNum) { Initialize(deviceNum); }
void Joystick::Initialize(std::string deviceNum) {
  std::string deviceName("hid.usb" + deviceNum);
  std::filesystem::path deviceDir(g_GadgetPath / "functions" / deviceName);
  if (!std::filesystem::exists(deviceDir)) {
    std::filesystem::create_directories(deviceDir);
    WriteFile file;
    ConfigMap Configs;
    {
      Configs["protocol"] = "0";
      Configs["report_length"] = "6";
      Configs["subclass"] = "0";
      Configs["report_desc"] =
          "05010904A1011581257F0901A10009300931750895028102C0A10005091901292015"
          "002501750195208102C0C0";

      for (auto [name, value] : Configs) {
        if (file.good()) {
          file.open(deviceDir / name);
          file << value;
          file.close();
        } else
          return;
      }
    }
    std::filesystem::create_directory_symlink(
        deviceDir,
        std::filesystem::path(g_GadgetPath / "configs/c.1/" / deviceName));
  }
}
Joystick::~Joystick() {}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ EOF