-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- "variable v_result : string(1 to get_len(val))", filled with
-- "(others => ' ')", where val is an array only running sizes. The fill
-- worked its length out again from the bounds, as a call, and the back end
-- stopped asking for that call to be a constant. UVVM's to_string of a
-- t_slv_array is the shape (uvvm_util/src/string_methods_pkg.vhd).
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_strfillcall is
end entity;

architecture a of t_vhdl_strfillcall is
  type t_slv_array is array (natural range <>) of std_logic_vector;

  function to_string(val : t_slv_array) return string is
    function get_len(value : t_slv_array) return integer is
      variable n : integer := 2;
    begin
      for i in value'range loop
        n := n + value(i)'length + 1;
      end loop;
      return n;
    end function;
    variable v_result : string(1 to get_len(val));
    variable k : integer := 2;
  begin
    v_result := (others => ' ');
    v_result(1) := '(';
    for i in val'range loop
      for j in val(i)'range loop
        v_result(k) := std_logic'image(val(i)(j))(2);
        k := k + 1;
      end loop;
      k := k + 1;
    end loop;
    v_result(v_result'right) := ')';
    return v_result;
  end function;

  signal tuser : std_logic_vector(2 downto 0) := "101";

  procedure expect(constant ex : t_slv_array; signal tu : in std_logic_vector;
                   variable s : out string) is
    constant nb : natural := tu'length;
    variable rx : t_slv_array(ex'range)(nb - 1 downto 0);
  begin
    for w in rx'range loop
      rx(w) := tu;
    end loop;
    s := to_string(rx);
  end procedure;
begin
  process
    variable big : t_slv_array(0 to 7)(15 downto 0);
    variable cnt : natural := 3;
    variable s : string(1 to 14);
  begin
    wait for 1 ns;
    expect(big(0 to cnt - 1), tuser, s);
    assert s = "(101 101 101 )" report s severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
