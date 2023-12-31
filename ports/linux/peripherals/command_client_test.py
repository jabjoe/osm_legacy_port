#!/usr/bin/env python3


import command_server


def main():
    with command_server.command_command_client_t("/tmp/osm_2/command_socket") as cmd_clt:
        print(cmd_clt.get_blocking("VEML7700", ["LUX"]))
        print(cmd_clt.set_blocking("VEML7700", {"LUX": 100}))
        print(cmd_clt.get_blocking("VEML7700", ["LUX"]))
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
