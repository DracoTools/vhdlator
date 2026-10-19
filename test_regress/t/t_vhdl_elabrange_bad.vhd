-- DESCRIPTION: Verilator: VHDL run-time check: generics given values elaboration folds outside their subtypes (6.5.6.2; GHDL gna issue3177)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_elabrange_sub is
  generic (n : natural := 1);
end entity;

architecture a of t_vhdl_elabrange_sub is
begin
end architecture;

entity t_vhdl_elabrange_bad is
end entity;

architecture a of t_vhdl_elabrange_bad is
  function m1 return integer is
  begin
    return -1;
  end function;
begin
  b1 : block
    generic (v : natural);
    generic map (v => m1);
  begin
  end block;
  u1 : entity work.t_vhdl_elabrange_sub generic map (n => m1);
  process is
  begin
    report "the model should have stopped when it started" severity failure;
    wait;
  end process;
end architecture;
