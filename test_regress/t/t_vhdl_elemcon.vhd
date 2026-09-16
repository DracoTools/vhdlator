-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- VHDL-2008 lets an array type leave its element unconstrained, and then the
-- value an object is given says how long the elements are (5.3.2.2).
--
-- An "others" choice filling such an array is accepted here because GHDL
-- 7.0.0-dev accepts it and its own testsuite requires it (gna/issue1055,
-- which is analyzed, elaborated and run); nvc 1.22.1 and Questa 2025.2 both
-- refuse it, holding that the element length cannot be told from the
-- context. A corpus that says a source must compile outranks a majority of
-- references that refuse it.

entity t_vhdl_elemcon is
end entity;

use std.env.all;

architecture tb of t_vhdl_elemcon is
  type arg_t is array (natural range <>) of bit_vector;
  constant two : bit_vector(1 downto 0) := "01";
  -- The element is as long as what fills it
  constant filled : arg_t(0 to 2) := (others => two);
  -- And as long as what is written out element by element
  constant listed : arg_t(0 to 1) := ("1010", "0101");
  -- A null array of such a type has no elements to measure at all
  constant empty : arg_t(0 downto 1) := (others => two);

  -- Where the object's own subtype leaves the index open as well, the value
  -- says how long both of them are. Taking only the index from it and
  -- leaving the element as the type declared it gives an object whose
  -- elements are the wrong length -- nvc 1.22.1, GHDL 7.0.0-dev and Questa
  -- 2025.2 all measure this one as four.
  constant sized : arg_t(1 downto 0)(3 downto 0) := (others => "1100");
  constant fromvalue : arg_t := sized;
begin

  process is
  begin
    assert filled(0) = "01" report "filled" severity failure;
    assert filled(2)'length = 2 report "filled length" severity failure;
    assert listed(1) = "0101" report "listed" severity failure;
    assert empty'length = 0 report "empty" severity failure;
    assert fromvalue'length = 2 report "fromvalue count" severity failure;
    assert fromvalue(0)'length = 4 report "fromvalue element" severity failure;
    assert fromvalue(1) = "1100" report "fromvalue value" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
