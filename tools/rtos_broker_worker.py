#!/usr/bin/env python3
"""Private local broker worker; configuration arrives through stdin, never argv.

The console owns this process. Terminating it deliberately models broker loss
and lets the OS close every accepted socket, including incomplete MQTT logins.
Parent stdin EOF also exits, so a closed console cannot leave an orphan broker.
"""
import asyncio
import json
import logging
import os
import sys
import warnings


async def run(config):
    from amqtt.broker import Broker
    with warnings.catch_warnings():
        warnings.simplefilter('ignore',DeprecationWarning)
        warnings.filterwarnings('ignore',message="The 'acl' option is deprecated.*",category=UserWarning)
        broker=Broker(config)
    await broker.start()
    print('BROKER_READY',flush=True)
    await asyncio.to_thread(sys.stdin.read)
    # Do not wait on library shutdown: unregistered TCP clients may prevent
    # asyncio.Server.wait_closed() from ever finishing on Python >=3.12.
    os._exit(0)


if __name__=='__main__':
    logging.basicConfig(level=logging.ERROR)
    asyncio.run(run(json.loads(sys.stdin.readline())))
