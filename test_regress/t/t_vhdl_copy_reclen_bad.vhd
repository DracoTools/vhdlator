-- DESCRIPTION: Verilator: VHDL run-time check: a record element of another length than the one a call gave (NVC bounds33)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_copy_reclen_bad is
end entity;

architecture a of t_vhdl_copy_reclen_bad is
  type rec is record
    x : bit_vector;
  end record;
  procedure assign(p : out rec) is
  begin
    p := (x => "1111");
  end procedure;
begin
  process is
    variable s4 : rec(x(1 to 4));
    variable s3 : rec(x(1 to 3));
  begin
    assign(s4);
    assert s4.x = "1111" severity failure;
    report "four fits";
    assign(s3);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
