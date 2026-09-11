-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An access type whose designated type is itself an access type, allocated
-- as the value of another allocation: Verilog's "new" stands only as an
-- assignment's value, so the inner object is made into a temporary first.

entity t_vhdl_accessacc is
end entity;

architecture a of t_vhdl_accessacc is
begin
  process
    type int_ptr is access integer;
    type ptr_ptr is access int_ptr;
    variable q : ptr_ptr;
    variable p : int_ptr;
  begin
    q := new int_ptr'(new integer'(5));
    assert q.all.all = 5 report "wrong nested value" severity failure;
    q := new int_ptr;
    assert q.all = null report "new int_ptr should be null" severity failure;
    p := new integer'(7);
    q.all := p;
    assert q.all.all = 7 report "wrong via assignment" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
