-- DESCRIPTION: Verilator: VHDL attribute of a statement label read back
--
-- "attribute cell_position of the_fpu : label is (540, 1200)" decorates the
-- block statement labelled the_fpu, which is declared after the
-- specification, and "the_fpu'cell_position" reads the value back from a
-- process of the same architecture -- as it does for an instance label and
-- for a process label (7.2, 16.2; VESTS ashenden ch_20_ch_20_03).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_attrlabel_sub is
end entity;

architecture a of t_vhdl_attrlabel_sub is
begin
end architecture;

entity t_vhdl_attrlabel is
end entity;

architecture a of t_vhdl_attrlabel is
  type coordinate is record
    x, y : integer;
  end record;
  attribute cell_position : coordinate;
  attribute cell_name : string;
  attribute cell_position of the_fpu : label is (540, 1200);
  attribute cell_name of u1 : label is "SUB";
  attribute cell_name of checker : label is "CHK";
begin
  the_fpu : block is
  begin
  end block;

  u1 : entity work.t_vhdl_attrlabel_sub;

  checker : process is
    variable p : coordinate;
  begin
    p := the_fpu'cell_position;
    assert p.x = 540 and p.y = 1200 severity failure;
    assert u1'cell_name = "SUB" severity failure;
    assert checker'cell_name = "CHK" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
