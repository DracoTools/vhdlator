-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- RESIZE of a parameter held with its bounds, to a width an attribute of a
-- signal parameter gives: "std_logic_vector(resize(addr_value,
-- paddr'length))" in a bus-functional procedure. A call whose data is a
-- function result only running sizes has the procedure hold its other
-- vector parameters with their bounds too, and RESIZE to a width that is
-- not a number was written as a cast -- of a carrier, which the back end
-- refused ("Size-changing cast on non-basic data type").
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
package t_vhdl_resizecarried_pkg is
  impure function random(n : natural) return std_logic_vector;
  procedure bus_write(constant addr_value : in unsigned; constant data_value : in std_logic_vector;
                      constant byte_enable : in std_logic_vector;
                      constant protection : in std_logic_vector(2 downto 0); constant msg : in string;
                      signal pclk : in std_logic; signal paddr : inout std_logic_vector;
                      signal pwdata : inout std_logic_vector; signal pstrb : inout std_logic_vector;
                      constant scope : in string := "BFM");
end package;
package body t_vhdl_resizecarried_pkg is
  impure function random(n : natural) return std_logic_vector is
    variable r : std_logic_vector(n - 1 downto 0);
    variable s : natural := 7;
  begin
    for i in r'range loop
      s := (s * 13 + 5) mod 17;
      r(i) := '1' when s > 8 else '0';
    end loop;
    return r;
  end function;
  procedure bus_write(constant addr_value : in unsigned; constant data_value : in std_logic_vector;
                      constant byte_enable : in std_logic_vector;
                      constant protection : in std_logic_vector(2 downto 0); constant msg : in string;
                      signal pclk : in std_logic; signal paddr : inout std_logic_vector;
                      signal pwdata : inout std_logic_vector; signal pstrb : inout std_logic_vector;
                      constant scope : in string := "BFM") is
    variable v_addr : std_logic_vector(paddr'length - 1 downto 0) := std_logic_vector(resize(addr_value, paddr'length));
    variable v_data : std_logic_vector(pwdata'length - 1 downto 0) := data_value;
    variable v_be : std_logic_vector(pstrb'length - 1 downto 0) := byte_enable;
  begin
    wait until rising_edge(pclk);
    paddr <= v_addr;
    pwdata <= v_data;
    pstrb <= v_be;
    wait until rising_edge(pclk);
    assert paddr = std_logic_vector(resize(addr_value, paddr'length)) severity failure;
    assert pwdata = data_value severity failure;
    assert pstrb = byte_enable severity failure;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_resizecarried_pkg.all;
entity t_vhdl_resizecarried is
  generic (GC_DATA_WIDTH : natural := 32; GC_ADDR_WIDTH : natural := 32);
end entity;
architecture a of t_vhdl_resizecarried is
  signal pstrb : std_logic_vector(GC_DATA_WIDTH / 8 - 1 downto 0);
  signal paddr : std_logic_vector(GC_ADDR_WIDTH - 1 downto 0);
  signal pwdata : std_logic_vector(GC_DATA_WIDTH - 1 downto 0);
  signal clk : std_logic := '0';
begin
  clk <= not clk after 5 ns;
  process
    variable v_strb : std_logic_vector(GC_DATA_WIDTH / 8 - 1 downto 0) := (others => '1');
    variable v_wr_data : std_logic_vector(GC_DATA_WIDTH - 1 downto 0);
    variable v_protection : std_logic_vector(2 downto 0) := (others => '0');
    procedure bus_write(addr : in natural; data : in std_logic_vector; byte_enable : in std_logic_vector;
                        protection : in std_logic_vector; msg : in string) is
    begin
      bus_write(to_unsigned(addr, GC_ADDR_WIDTH), data, byte_enable, protection, msg, clk, paddr, pwdata, pstrb);
    end procedure;
  begin
    v_wr_data := random(GC_DATA_WIDTH);
    bus_write(8, v_wr_data, v_strb, v_protection, "first");
    v_strb := (others => '0');
    bus_write(8, random(GC_DATA_WIDTH), v_strb, v_protection, "random");
    assert pstrb = "0000" and unsigned(paddr) = 8 severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
