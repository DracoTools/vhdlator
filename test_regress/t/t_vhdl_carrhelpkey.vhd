-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- One procedure called with arrays of eight-bit and of sixteen-bit words,
-- each sliced where only running settles the ends and handed on. The words'
-- lengths make two carrier structures, but the helper that slices one was
-- kept by the array type alone, so the second call used the first's, and
-- the C++ compiler refused the mismatch. UVVM's axistream VVC testbench is
-- the shape (bitvis_vip_axistream/tb/maintenance_tb/
-- axistream_vvc_slv_array_tb.vhd).
library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_carrhelpkey is end entity;
architecture a of t_vhdl_carrhelpkey is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  procedure consume(signal tgt : inout std_logic; constant d : in t_slv_array; variable n : out natural; variable last : out std_logic_vector) is
  begin
    tgt <= '1';
    n := d'length;
    last := d(d'right);
  end procedure;
  procedure outer(signal tgt : inout std_logic; constant data_array : in t_slv_array; numBytes : integer; variable n : out natural; variable last : out std_logic_vector) is
    variable v_numBytes : integer := numBytes;
  begin
    v_numBytes := v_numBytes - 1;
    consume(tgt, data_array(0 to v_numBytes - 1), n, last);
  end procedure;
  signal tgt : std_logic := '0';
begin
  process
    variable big : t_slv_array(0 to 7)(7 downto 0) := (x"01", x"02", x"03", x"04", others => x"ff");
    variable k : natural := 5;
    variable n : natural;
    variable last : std_logic_vector(7 downto 0);
    variable big2 : t_slv_array(0 to 7)(15 downto 0) := (x"0101", x"0202", x"0303", others => x"ffff");
    variable last2 : std_logic_vector(15 downto 0);
  begin
    outer(tgt, big2(0 to k - 1), 3, n, last2);
    assert n = 2 and last2 = x"0202" severity failure;
    outer(tgt, big(0 to k - 1), 4, n, last);
    report integer'image(n) & " " & to_hstring(last);
    assert n = 3 and last = x"03" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
