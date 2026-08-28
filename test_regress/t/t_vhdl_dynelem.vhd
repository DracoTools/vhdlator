-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_dynelem is
end entity;

architecture a of t_vhdl_dynelem is

  type t_counts is array (natural range <>) of integer;
  type t_counts_ptr is access t_counts;

  type t_slot is record
    name  : integer;
    tally : t_counts_ptr;   -- bounds only the allocation knows
  end record;

  type t_slots is array (0 to 3) of t_slot;

begin

  main : process
    variable reg : t_slots;
    variable total : integer := 0;
  begin
    for i in 0 to 3 loop
      reg(i).name := i;
      -- The access value is reached through an index and a selection, not
      -- by a plain name
      reg(i).tally := new t_counts(0 to i);
      for j in reg(i).tally'range loop
        reg(i).tally(j) := i * 10 + j;
      end loop;
    end loop;

    for i in 0 to 3 loop
      assert reg(i).tally'length = i + 1
        report "length wrong at " & integer'image(i) severity failure;
      for j in 0 to i loop
        total := total + reg(i).tally(j);
      end loop;
    end loop;

    -- 0 + (10+11) + (20+21+22) + (30+31+32+33) = 0 + 21 + 63 + 126
    assert total = 210 report "total is " & integer'image(total) severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
