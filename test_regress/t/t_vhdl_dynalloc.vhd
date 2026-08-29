-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_dynalloc is
end entity;

architecture a of t_vhdl_dynalloc is

  type int_vec is array (natural range <>) of integer;
  type int_vec_ptr is access int_vec;

begin

  main : process
    variable p : int_vec_ptr;
    variable n : integer := 3;
    variable total : integer := 0;
  begin
    -- The range of the aggregate the allocator is given is only known
    -- while running, and every element of it is the one value (9.3.7)
    p := new int_vec'(0 to n + 1 => 7);
    assert p'length = 5 report "length is " & integer'image(p'length) severity failure;
    assert p'left = 0 report "left" severity failure;
    for i in p'range loop
      total := total + p(i);
    end loop;
    assert total = 35 report "total is " & integer'image(total) severity failure;
    deallocate(p);

    -- and a descending one
    n := 6;
    p := new int_vec'(n downto 2 => 1);
    assert p'length = 5 report "length 2 is " & integer'image(p'length) severity failure;
    assert p'left = 6 report "left 2" severity failure;
    assert p(4) = 1 report "element" severity failure;
    deallocate(p);

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
