-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An architecture's constant of a record type, given what pure functions
-- decode from a string, is what elaborating it gives: its elements choose a
-- generate branch and size a signal (PoC's "constant DEV_INFO :
-- T_DEVICE_INFO := DEVICE_INFO" in sync_Bits)

package t_vhdl_devinfo_pkg is
  type vendor_t is (VENDOR_UNKNOWN, VENDOR_GENERIC, VENDOR_XILINX);
  type info_t is record
    vendor : vendor_t;
    fanin  : positive;
    number : natural;
  end record;
  constant MY_DEVICE : string := "XC7K325T";
  function device_info(s : string := "") return info_t;
end package;

package body t_vhdl_devinfo_pkg is
  function pick(s : string) return string is
  begin
    if s'length = 0 then
      return MY_DEVICE;
    end if;
    return s;
  end function;
  -- A table of the package's, which only elaboration reads
  type prefix_t is record
    prefix : string(1 to 2);
    vendor : vendor_t;
  end record;
  type prefixes_t is array (natural range <>) of prefix_t;
  constant PREFIXES : prefixes_t := (("GE", VENDOR_GENERIC), ("XC", VENDOR_XILINX));
  function vendor(s : string) return vendor_t is
    constant d : string := pick(s);
  begin
    for i in PREFIXES'range loop
      if d(d'left to d'left + 1) = PREFIXES(i).prefix then
        return PREFIXES(i).vendor;
      end if;
    end loop;
    return VENDOR_UNKNOWN;
  end function;
  function number(s : string) return natural is
    constant d : string := pick(s);
    variable n : natural := 0;
  begin
    for i in d'left + 4 to d'right loop
      exit when d(i) < '0' or d(i) > '9';
      n := n * 10 + character'pos(d(i)) - character'pos('0');
    end loop;
    return n;
  end function;
  function device_info(s : string := "") return info_t is
    variable r : info_t;
  begin
    r.vendor := vendor(s);
    r.number := number(s);
    if r.vendor = VENDOR_XILINX then
      r.fanin := 6;
    else
      r.fanin := 4;
    end if;
    return r;
  end function;
end package body;

use work.t_vhdl_devinfo_pkg.all;
entity t_vhdl_devinfo is
end entity;
architecture a of t_vhdl_devinfo is
  constant DEV_INFO : info_t := device_info;
  signal lut : bit_vector(DEV_INFO.fanin - 1 downto 0);
  signal picked : natural := 0;
  signal other : natural := 0;
begin
  gx : if DEV_INFO.vendor = VENDOR_XILINX generate
    picked <= 1;
  end generate;
  gg : if DEV_INFO.vendor /= VENDOR_XILINX generate
    other <= 2;
  end generate;
  process
  begin
    wait for 1 ns;
    assert picked = 1 and other = 0
      report "picked " & integer'image(picked) & integer'image(other) severity failure;
    assert lut'length = 6 report "lut length " & integer'image(lut'length) severity failure;
    assert DEV_INFO.number = 325
      report "number " & integer'image(DEV_INFO.number) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
