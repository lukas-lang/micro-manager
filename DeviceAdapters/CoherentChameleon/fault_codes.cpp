///////////////////////////////////////////////////////////////////////////////
// FILE:          fault_codes.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   A device adapter for the Coherent Chameleon Ultra laser
//
// AUTHOR:        Lukas Lang
//
// COPYRIGHT:     2017 Lukas Lang
// LICENSE:       Licensed under the Apache License, Version 2.0 (the "License");
//                you may not use this file except in compliance with the License.
//                You may obtain a copy of the License at
//                
//                http://www.apache.org/licenses/LICENSE-2.0
//                
//                Unless required by applicable law or agreed to in writing, software
//                distributed under the License is distributed on an "AS IS" BASIS,
//                WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//                See the License for the specific language governing permissions and
//                limitations under the License.

#include "fault_codes.h"

#include "Util.h"

const std::map<int, std::string> fault_codes = map_of
(0, "no faults")
(1, "Laser Head Interlock Fault")
(2, "External Interlock Fault")
(3, "PS Cover Interlock Fault")
(4, "LBO Temperature Fault")
(5, "LBO Not Locked at Set Temp")
(6, "Vanadate Temp. Fault")
(7, "Etalon Temp. Fault")
(8, "Diode 1 Temp. Fault")
(9, "Diode 2 Temp. Fault")
(10, "Baseplate Temp. Fault")
(11, "Heatsink 1 Temp. Fault")
(12, "Heatsink 2 Temp. Fault")
(16, "Diode 1 Over Current Fault")
(17, "Diode 2 Over Current Fault")
(18, "Over Current Fault")
(19, "Diode 1 Under Volt Fault")
(20, "Diode 2 Under Volt Fault")
(21, "Diode 1 Over Volt Fault")
(22, "Diode 2 Over Volt Fault")
(25, "Diode 1 EEPROM Fault")
(26, "Diode 2 EEPROM Fault")
(27, "Laser Head EEPROM Fault")
(28, "PS EEPROM Fault")
(29, "PS-Head Mismatch Fault")
(30, "LBO Battery Fault")
(31, "Shutter State Mismatch")
(32, "CPU PROM Checksum Fault")
(33, "Head PROM Checksum Fault")
(34, "Diode 1 PROM Checksum Fault")
(35, "Diode 2 PROM Checksum Fault")
(36, "CPU PROM Range Fault")
(37, "Head PROM Range Fault")
(38, "Diode 1 PROM Range Fault")
(39, "Diode 2 PROM Range Fault")
(40, "Head - Diode Mismatch")
(43, "Lost Modelock Fault")
(47, "Ti-Sapph Temp. Fault")
(49, "PZT X Fault")
(50, "Cavity Humidity Fault")
(51, "Tuning Stepper Motor Homing")
(52, "Lasing Fault")
(53, "Laser Failed to Begin Modelocking")
(54, "Headboard Communication Fault")
(55, "System Lasing Fault")
(56, "PS-Head EEPROM Mismatch Fault")
(57, "Modelock Slit Stepper Motor Homing Fault")
(58, "Chameleon VERDIPROM Fault")
(59, "Chameleon Precompensator Homing Fault")
(60, "Chameleon CURVEEPROM Fault");