-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, what the UVVM and VUnit libraries do that the
-- battery did not: an element of a package's constant vector read in a
-- package subprogram, an index in an assignment's target that warns about
-- metavalues, a slice whose ends only running settles of a record signal
-- written through a procedure, and a transaction preempted on a resolved
-- signal

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_nine_lib_pkg is
  constant C_KIND : std_logic_vector(1 downto 0) := "10";
  function kind_bit(i : natural) return std_logic;
  type if_t is record
    tdata  : std_logic_vector;
    tuser  : std_logic_vector;
    tvalid : std_logic;
  end record;
  type t_user_array is array (natural range <>) of std_logic_vector(7 downto 0);
  procedure send(signal ifc : inout if_t; constant user_array : in t_user_array);
end package;

package body t_vhdl_nine_lib_pkg is
  function kind_bit(i : natural) return std_logic is
  begin
    return C_KIND(i);
  end function;

  procedure send(signal ifc : inout if_t; constant user_array : in t_user_array) is
    constant C_BITS  : natural := ifc.tuser'length;
    constant C_BYTES : natural := ifc.tdata'length / 8;
  begin
    for byte in 0 to user_array'high loop
      ifc.tvalid <= '1';
      ifc.tuser(C_BITS - 1 downto 0) <= user_array(byte / C_BYTES)(C_BITS - 1 downto 0);
      wait for 1 ns;
    end loop;
    ifc.tvalid <= '0';
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_nine_lib_pkg.all;

entity t_vhdl_nine_lib is
end entity;

architecture a of t_vhdl_nine_lib is
  signal ifc  : if_t(tdata(15 downto 0), tuser(1 downto 0));
  signal regs : std_logic_vector(3 downto 0) := "0000";
  signal res  : std_logic_vector(3 downto 0) := "ZZZZ";
begin
  -- Two drivers of a resolved signal, one preempting its own transaction
  process
  begin
    res <= "1111" after 10 ns;
    res <= "ZZ01" after 5 ns;
    wait;
  end process;
  process
  begin
    res <= "10ZZ" after 3 ns;
    wait;
  end process;

  process
    variable idx : unsigned(1 downto 0) := "10";
  begin
    report "kind " & std_logic'image(kind_bit(0)) & std_logic'image(kind_bit(1));
    regs(to_integer(idx)) <= '1';
    idx := "0X";
    regs(to_integer(idx)) <= 'H';
    wait for 0 ns;
    report "regs " & to_string(regs);
    send(ifc, (x"02", x"03"));
    report "tuser " & to_string(ifc.tuser) & " tvalid " & std_logic'image(ifc.tvalid);
    wait for 10 ns;
    report "res " & to_string(res);
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
