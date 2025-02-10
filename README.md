# NXP Application Code Hub
[<img src="https://mcuxpresso.nxp.com/static/icon/nxp-logo-color.svg" width="100"/>](https://www.nxp.com)

## AN14434 How to implement MCXN236 USB to I3C demo 

This is the complementary project for [AN14434](https://www.nxp.com.cn/docs/en/application-note/AN14434.pdf), which demonstrates how implement usb to i3c demo on MCXN236.  can use terminal tool to send a serial data to control I3C interface.

In the MCXN236 USB to I3C demo, the USB device uses USB CDC virtual com class to communicate with PC host. 

You can use terminal tool to send a serial data to control I3C interface.

The terminal tool used in following contents is pzh-py-com tool. Customer can download it from followed link: https://github.com/JayHeng/pzh-py-com.

This demo provided some commands such as Dynamic address assign, direct write, direct read, write with register address, read with register address, IBI/Hot-join functions.

[<img src="./images/MCXN236_USB_TO_I3C_Block.png" align="center" width="800"/>](./images/MCXN236_USB_TO_I3C_Block.png)

#### Boards: FRDM-MCXN236
#### Categories: HMI
#### Peripherals: I3C, USB
#### Toolchains: MCUXpresso IDE

## Table of Contents
1. [Software](#step1)
2. [Hardware](#step2)
3. [Setup](#step3)
4. [Results](#step4)
5. [FAQs](#step5) 
6. [Support](#step6)
7. [Release Notes](#step7)

## 1. Software<a name="step1"></a>

The software for this Application Note is delivered in raw source files and MCUXpresso projects. Software version:

- SDK: v2.16.0
- IDE: MCUXpresso IDE v11.9.0

## 2. Hardware<a name="step2"></a>

- USB Type-C cable
- FRDM-MCXN236 board (https://www.nxp.com/design/design-center/development-boards-and-designs/general-purpose-mcus/frdm-development-board-for-mcx-n23x-mcus:FRDM-MCXN236)
- Personal Computer

[<img src="./images/FRDM-MCXN236.png" align="center" width="800"/>](./images/FRDM-MCXN236.png)

## 3. Hardware Setup<a name="step3"></a>

To perform the MCXN236 USB to I3C demo, use two FRDM-MCXN236 boards one as I3C controller another was used as I3C target.

This demo uses P1_16(I3C_SDA) and P1_17(I3C_SCL) pins as i3c function. 

For hardware connection, please refer below Figure. The length of connect wires should be as short as possible.

[<img src="./images/hardware_setup.png" align="center" high="400" width="400"/>](./images/hardware_setup.png)

### 3.1 Software Step

1. Open MCUXpresso IDE, in the Quick Start Panel, choose **Import from Application Code Hub**    

    [<img src="./images/import_project_1.png" align="center" high="350" width="350"/>](./images/import_project_1.png)

2. Enter the demo name in the search bar.   

    [<img src="./images/import_project_2.png" align="center" high="350" width="350"/>](./images/import_project_2.png)


3. Click **Copy GitHub link**, MCUXpresso IDE will automatically retrieve project attributes, then click **Next>**.   

    [<img src="./images/import_project_3.png" align="center" high="350" width="350"/>](./images/import_project_3.png)


4. Select **main** branch and then click **Next>**, Select the MCUXpresso project, click **Finish** button to complete import.   

    [<img src="./images/import_project_4.png" align="center" high="350" width="350"/>](./images/import_project_4.png)

5. Click **Build** to start compiling the project.

## 4. Results<a name="step4"></a>

Please follow the Application Note ([AN14434](https://www.nxp.com.cn/docs/en/application-note/AN14434.pdf)) to perform the USB to I3C bridge demo. You can use virtual com terminal tool to send I3C control command.

Below lists some commands.

- When the terminal send List DAA command, it will receive I3C Target information feedback which contains the target vendor ID and BCR/DCR values. 

    [<img src="./images/LDAA.png" align="center" high="400" width="400"/>](./images/LDAA.png)

- When the terminal send write without register address command. After command finished, the terminal will receive OK(0x4F, 0x4B) characters

    [<img src="./images/Write.png" align="center" high="400" width="400"/>](./images/Write.png)

- When the terminal send read without register address command. After command finished, the terminal will receive data which were sent by target.

    [<img src="./images/Read.png" align="center" high="400" width="400"/>](./images/Read.png)

- When the terminal send read with register address command. After command finished, the terminal will receive data which were sent by target.

    [<img src="./images/read_with.png" align="center" high="400" width="400"/>](./images/read_with.png)

## 5. FAQs<a name="step5"></a>

No FAQs have been identified for this project.

## 6. Support<a name="step6"></a>

Please open a issue for support.

#### Project Metadata

<!----- Boards ----->
[![Board badge](https://img.shields.io/badge/Board-FRDM&ndash;MCXN236-blue)]()

<!----- Categories ----->
[![Category badge](https://img.shields.io/badge/Category-HMI-yellowgreen)](https://github.com/search?q=org%3Anxp-appcodehub+hmi+in%3Areadme&type=Repositories)

<!----- Peripherals ----->
[![Peripheral badge](https://img.shields.io/badge/Peripheral-I3C-yellow)](https://github.com/search?q=org%3Anxp-appcodehub+i3c+in%3Areadme&type=Repositories)
[![Peripheral badge](https://img.shields.io/badge/Peripheral-USB-yellow)](https://github.com/search?q=org%3Anxp-appcodehub+usb+in%3Areadme&type=Repositories)

<!----- Toolchains ----->
[![Toolchain badge](https://img.shields.io/badge/Toolchain-MCUXPRESSO%20IDE-orange)](https://github.com/search?q=org%3Anxp-appcodehub+mcux+in%3Areadme&type=Repositories)

Questions regarding the content/correctness of this example can be entered as Issues within this GitHub repository.

>**Warning**: For more general technical questions regarding NXP Microcontrollers and the difference in expected functionality, enter your questions on the [NXP Community Forum](https://community.nxp.com/)

[![Follow us on Youtube](https://img.shields.io/badge/Youtube-Follow%20us%20on%20Youtube-red.svg)](https://www.youtube.com/NXP_Semiconductors)
[![Follow us on LinkedIn](https://img.shields.io/badge/LinkedIn-Follow%20us%20on%20LinkedIn-blue.svg)](https://www.linkedin.com/company/nxp-semiconductors)
[![Follow us on Facebook](https://img.shields.io/badge/Facebook-Follow%20us%20on%20Facebook-blue.svg)](https://www.facebook.com/nxpsemi/)
[![Follow us on Twitter](https://img.shields.io/badge/X-Follow%20us%20on%20X-black.svg)](https://x.com/NXP)

## 7. Release Notes<a name="step7"></a>
| Version | Description / Update                           | Date                        |
|:-------:|------------------------------------------------|----------------------------:|
| 1.0     | Initial release on Application Code Hub        | October 9<sup>th</sup> 2024 |

## Licensing

*If applicable - note software licensing here with links to licenses, otherwise remove this section*

## Origin

*if applicable - note components your application uses regarding to license terms - with authors / licenses / links to licenses, otherwise remove this section*