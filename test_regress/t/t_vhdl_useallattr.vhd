-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An attribute a design unit gave itself, read from another unit through "use work.all" though the attribute's declaration is not visible there (7.2, 12.4; GHDL gna issue1528)

package t_vhdl_useallattr_pkg is
  attribute ent_name : string;
  attribute ent_stat : integer;
end package;

use work.t_vhdl_useallattr_pkg.all;
entity t_vhdl_useallattr_ent is
  attribute ent_name of t_vhdl_useallattr_ent : entity is "entity one";
  attribute ent_stat of t_vhdl_useallattr_ent : entity is 95;
end entity;

architecture a of t_vhdl_useallattr_ent is
begin
end architecture;

use work.all;
entity t_vhdl_useallattr is
end entity;

architecture a of t_vhdl_useallattr is
begin
  process is
  begin
    assert t_vhdl_useallattr_ent'ent_name = "entity one" severity failure;
    assert t_vhdl_useallattr_ent'ent_stat = 95 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
