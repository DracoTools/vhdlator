-- DESCRIPTION: Verilator: VHDL elaboration check: an external name evaluated before its object is elaborated (8.7; GHDL gna issue2528)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_ename_early_sub is
end entity;

architecture a of t_vhdl_ename_early_sub is
  signal data : integer := 5;
begin
end architecture;

entity t_vhdl_ename_early_bad is
end entity;

architecture a of t_vhdl_ename_early_bad is
  -- Elaborated with the declarations, before the instance below
  alias probe is <<signal .t_vhdl_ename_early_bad.u.data : integer>>;
begin
  u : entity work.t_vhdl_ename_early_sub;
  b : block
    -- Elaborated after the instance, which is fine
    alias later is <<signal .t_vhdl_ename_early_bad.u.data : integer>>;
  begin
  end block;
  process is
  begin
    assert probe = 5 severity failure;
    report "the model should have stopped when it started" severity failure;
    wait;
  end process;
end architecture;
