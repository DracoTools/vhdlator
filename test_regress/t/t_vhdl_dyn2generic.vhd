-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A function giving back an array of arrays sized by what it is handed,
-- assigned to a parameter of a procedure that waits, whose actual has as
-- many words as a generic says (UVVM's axistream_receive, called directly
-- with an array of 32-bit words: "data_array :=
-- convert_byte_array_to_slv_array(...)"). The count is settled with the
-- instance, not when the procedure is compiled, and the words go into the
-- actual's places from its left, whichever way it runs (10.6.2.1).

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_dyn2generic_pkg is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  subtype t_byte_array is t_slv_array(open)(7 downto 0);
  impure function to_words(ba : t_byte_array; bpw : natural) return t_slv_array;
  procedure receive(variable data_array : inout t_slv_array; signal clk : in std_logic);
end package;

package body t_vhdl_dyn2generic_pkg is
  impure function to_words(ba : t_byte_array; bpw : natural) return t_slv_array is
    constant n : integer := ba'length / bpw;
    variable va : t_slv_array(0 to n - 1)((8 * bpw) - 1 downto 0);
    variable k : integer := 0;
  begin
    for w in 0 to n - 1 loop
      for b in bpw downto 1 loop
        va(w)((8 * b) - 1 downto (b - 1) * 8) := ba(k);
        k := k + 1;
      end loop;
    end loop;
    return va;
  end function;

  procedure receive(variable data_array : inout t_slv_array; signal clk : in std_logic) is
    constant C_BPW : natural := data_array(data_array'low)'length / 8;
    variable v_bytes : t_slv_array(0 to (data_array'length * C_BPW) - 1)(7 downto 0);
  begin
    wait until rising_edge(clk);
    for i in v_bytes'range loop
      v_bytes(i) := std_logic_vector(to_unsigned(i + 1, 8));
    end loop;
    data_array := to_words(v_bytes, C_BPW);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_dyn2generic_pkg.all;

entity t_vhdl_dyn2generic is
  generic (NBYTES : natural := 12);
end entity;

architecture a of t_vhdl_dyn2generic is
  signal clk : std_logic := '0';
begin
  clk <= not clk after 5 ns;

  process
    variable up : t_slv_array(0 to NBYTES / 4 - 1)(31 downto 0);
    variable dn : t_slv_array(NBYTES / 4 - 1 downto 0)(31 downto 0);
    variable bytes : t_slv_array(0 to 2)(7 downto 0);
  begin
    receive(up, clk);
    assert up(0) = x"01020304" and up(1) = x"05060708" and up(2) = x"090a0b0c"
      report "up gave " & to_hstring(up(0)) & " " & to_hstring(up(1)) & " "
             & to_hstring(up(2)) severity failure;
    receive(dn, clk);
    assert dn(2) = x"01020304" and dn(1) = x"05060708" and dn(0) = x"090a0b0c"
      report "dn gave " & to_hstring(dn(2)) & " " & to_hstring(dn(1)) & " "
             & to_hstring(dn(0)) severity failure;
    -- A count fixed where the procedure is compiled, as before
    receive(bytes, clk);
    assert bytes(0) = x"01" and bytes(2) = x"03"
      report "bytes gave " & to_hstring(bytes(0)) & " " & to_hstring(bytes(2))
      severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
