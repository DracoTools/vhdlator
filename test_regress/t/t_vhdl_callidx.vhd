-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package hold is
  type words is array (natural range <>) of integer;

  type keeper is protected
    procedure put(constant i : in integer; constant v : in integer);
    impure function all_of return words;
  end protected;

  -- and a plain function that takes nothing at all
  function table return words;
end package;

package body hold is
  type keeper is protected body
    variable priv : words(0 to 3) := (others => 0);
    procedure put(constant i : in integer; constant v : in integer) is
    begin
      priv(i) := v;
    end procedure;
    impure function all_of return words is
    begin
      return priv;
    end function;
  end protected body;

  function table return words is
    variable r : words(0 to 3);
  begin
    for i in r'range loop
      r(i) := 3 * i;
    end loop;
    return r;
  end function;
end package body;

use work.hold.all;

entity t_vhdl_callidx is
end entity;

architecture a of t_vhdl_callidx is
  shared variable k : keeper;
begin
  main : process is
  begin
    k.put(1, 42);
    k.put(2, 7);

    -- A function whose parameters all have defaults may be called by naming
    -- it, and then a following parenthesis indexes the result (4.2.2.1, 8.1)
    assert k.all_of(1) = 42 report "method index" severity failure;
    assert k.all_of(2) = 7 report "method index 2" severity failure;
    assert table(2) = 6 report "table(2) is " & integer'image(table(2)) severity failure;
    assert table(0) = 0 report "table(0)" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
