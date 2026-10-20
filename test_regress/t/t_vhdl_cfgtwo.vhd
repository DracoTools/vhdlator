-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Two configurations of one entity, the design elaborated through the one
-- that binds nothing but its test architecture: OSVVM's TbAxi4_Separate1 and
-- TbAxi4_Shared1. The other's "for all : mem ... generic map (MEMORY_NAME =>
-- "Shared")" was applied as well, so the memories meant to be separate shared
-- their storage.

entity t_vhdl_cfgtwo_mem is
  generic (MODEL_ID_NAME : string := ""; MEMORY_NAME : string := "");
end;
architecture a of t_vhdl_cfgtwo_mem is
begin
  process begin
    report MODEL_ID_NAME & " memory=" & MEMORY_NAME;
    assert MEMORY_NAME = "" severity failure;
    wait;
  end process;
end;

entity t_vhdl_cfgtwo_ctl is end;
architecture shared1 of t_vhdl_cfgtwo_ctl is
begin
  process begin
    report "wrong test architecture" severity failure;
    wait;
  end process;
end;
architecture separate1 of t_vhdl_cfgtwo_ctl is
begin
  process begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end;

entity t_vhdl_cfgtwo_h is end;
architecture th of t_vhdl_cfgtwo_h is
  component t_vhdl_cfgtwo_mem is
    generic (MODEL_ID_NAME : string := ""; MEMORY_NAME : string := "");
  end component;
  component t_vhdl_cfgtwo_ctl is end component;
begin
  m1 : t_vhdl_cfgtwo_mem generic map (MODEL_ID_NAME => "m1");
  m2 : t_vhdl_cfgtwo_mem generic map (MODEL_ID_NAME => "m2");
  c : t_vhdl_cfgtwo_ctl;
end;

configuration t_vhdl_cfgtwo_shared of t_vhdl_cfgtwo_h is
  for th
    for c : t_vhdl_cfgtwo_ctl use entity work.t_vhdl_cfgtwo_ctl(shared1); end for;
    for all : t_vhdl_cfgtwo_mem
      use entity work.t_vhdl_cfgtwo_mem
        generic map (MEMORY_NAME => "Shared", MODEL_ID_NAME => MODEL_ID_NAME);
    end for;
  end for;
end t_vhdl_cfgtwo_shared;

configuration t_vhdl_cfgtwo of t_vhdl_cfgtwo_h is
  for th
    for c : t_vhdl_cfgtwo_ctl use entity work.t_vhdl_cfgtwo_ctl(separate1); end for;
  end for;
end t_vhdl_cfgtwo;
