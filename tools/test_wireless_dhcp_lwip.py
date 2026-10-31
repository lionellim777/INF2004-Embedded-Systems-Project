#!/usr/bin/env python3
"""Compile the actual pinned lwIP with old/new firmware config; no mocked IP/UDP."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1];build=root/'build/wireless-lwip-tests';port=build/'port'
(port/'arch').mkdir(parents=True,exist_ok=True)
(port/'arch/cc.h').write_text('''typedef unsigned sys_prot_t;\n#include <stdio.h>\n#include <stdlib.h>\n#define LWIP_PLATFORM_DIAG(x) do {printf x;} while(0)\n#define LWIP_PLATFORM_ASSERT(x) do {fprintf(stderr,"lwIP assert: %s\\n",x);abort();} while(0)\n#define LWIP_RAND() ((unsigned)rand())\n''')
(port/'cyw43_config.h').write_text('#include <stdint.h>\nuint32_t cyw43_hal_ticks_ms(void);\n')
app=root/'firmware/app/robot_firmware/wireless';lwip=root/'build/dependencies/pico-sdk/lib/lwip/src'
files=sorted((lwip/'core').glob('*.c'))+sorted((lwip/'core/ipv4').glob('*.c'))+[lwip/'netif/ethernet.c',app/'dhcpserver.c',root/'firmware/tests/wireless_dhcp_lwip_test.c']
env=dict(os.environ,SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk')
for old in (True,False):
    opts=build/('old' if old else 'fixed');opts.mkdir(exist_ok=True)
    config=(app/'lwipopts.h').read_text()
    # Host pointers require 8-byte alignment; RP2040 uses 4. Network options identical.
    config=config.replace('#define MEM_ALIGNMENT 4', '#define MEM_ALIGNMENT 8')
    if old: config+='\n#undef LWIP_IP_ACCEPT_UDP_PORT\n'
    (opts/'lwipopts.h').write_text(config)
    cmd=['clang','-std=c11','-DLWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS=1','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-g','-I'+str(opts),'-I'+str(port),'-I'+str(lwip/'include'),'-I'+str(app)]
    if old: cmd+=['-DEXPECT_BROKEN_CONFIG=1']
    cmd+=list(map(str,files))+['-o',str(opts/'dhcp-test')]
    subprocess.run(cmd,env=env,check=True)
    subprocess.run([str(opts/'dhcp-test')],env=env,check=True)
