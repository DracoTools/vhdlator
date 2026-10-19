-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A relative pathname climbing above the root of the design names no region
-- at all (8.7): an error of elaboration (NVC ename17).

entity t_vhdl_ename_root_bad_sub is
end entity;

architecture behav of t_vhdl_ename_root_bad_sub is
begin
  process begin
    assert <<signal ^.^.^.^.x : bit>> = '0';
    wait;
  end process;
end architecture;

entity t_vhdl_ename_root_bad is
end entity;

architecture behav of t_vhdl_ename_root_bad is
begin
  uut : entity work.t_vhdl_ename_root_bad_sub;
end architecture;
