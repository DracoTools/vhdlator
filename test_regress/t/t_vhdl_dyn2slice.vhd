-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A slice, at an end only running settles, of an array whose elements
-- carry their own lengths, handed to a procedure with a signal parameter:
-- UVVM's "axistream_expect(AXISTREAM_VVCT, 1, data_array(0 to v_numBytes -
-- 1), ...)" (bitvis_vip_axistream/tb/maintenance_tb/
-- axistream_vvc_slv_array_tb.vhd), where data_array is what a function gave
-- back with words of a width a variable decides. The slice was refused as
-- an actual of such a parameter, and taken, where it was not, as if its
-- elements were of a fixed width, which the back end could not compile.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_dyn2slice_pkg is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  type t_user_array is array (natural range <>) of std_logic_vector(7 downto 0);
  signal count : integer := 0;
  procedure take(signal s : inout integer; constant i : integer; constant a : in t_slv_array;
                 constant u : in t_user_array; constant len : natural;
                 constant last : std_logic_vector);
  procedure take(signal s : inout integer; constant i : integer; constant a : in std_logic_vector;
                 constant u : in t_user_array; constant len : natural;
                 constant last : std_logic_vector);
end package;

package body t_vhdl_dyn2slice_pkg is
  procedure take(signal s : inout integer; constant i : integer; constant a : in t_slv_array;
                 constant u : in t_user_array; constant len : natural;
                 constant last : std_logic_vector) is
  begin
    assert a'length = len report "length " & integer'image(a'length) severity failure;
    assert a(a'right) = last report "last " & to_hstring(a(a'right)) severity failure;
    s <= s + 1;
  end procedure;

  procedure take(signal s : inout integer; constant i : integer; constant a : in std_logic_vector;
                 constant u : in t_user_array; constant len : natural;
                 constant last : std_logic_vector) is
  begin
    assert false report "the std_logic_vector overload was called" severity failure;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_dyn2slice_pkg.all;

entity t_vhdl_dyn2slice is
end entity;

architecture a of t_vhdl_dyn2slice is
begin
  process
    procedure check(data_array : t_slv_array; user_array : t_user_array; n : integer;
                    expect_last : std_logic_vector) is
      variable v_n : integer := n;
    begin
      v_n := v_n - 1;
      take(count, 1, data_array(0 to v_n - 1), user_array, n - 1, expect_last);
      take(count, 1, data_array, user_array, n, data_array(n - 1));
    end procedure;

    impure function get(n : integer; w : integer) return t_slv_array is
      variable r : t_slv_array(0 to n - 1)(w * 4 - 1 downto 0);
    begin
      for k in 0 to n - 1 loop
        r(k) := std_logic_vector(to_unsigned(k + 16 * w, w * 4));
      end loop;
      return r;
    end function;

    variable u : t_user_array(0 to 3) := (others => x"00");
    variable d : t_slv_array(0 to 3)(7 downto 0) := (x"11", x"22", x"33", x"44");
    variable nb : integer := 4;
    variable wv : integer := 3;
  begin
    -- Words of a width only running settles, twelve bits and then twenty
    check(get(nb, wv), u, nb, x"032");
    wv := 5;
    nb := 6;
    check(get(nb, wv), u, nb, x"00054");
    -- And of a width the text fixes
    check(d, u, 4, x"33");
    -- Every drive read count as it was before any of them took effect
    wait for 1 ns;
    assert count = 1 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
