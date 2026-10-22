-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A port and a generic of the entity given an attribute in the
-- architecture's declarative part, as Vivado's IP integrator wrappers write
-- "attribute X_INTERFACE_INFO of S_AXI_RREADY : signal is ..." (SLAC's
-- surf). nvc and Questa take it; GHDL only with -frelaxed.

entity t_vhdl_attrportarch_m is
  generic (name_g : string := "bus");
  port (rready : out bit; rvalid : in bit);
end entity;
architecture a of t_vhdl_attrportarch_m is
  attribute x_interface_info : string;
  attribute x_interface_info of rready : signal is "aximm " & name_g & " RREADY";
  attribute x_interface_info of rvalid : signal is "aximm " & name_g & " RVALID";
  attribute x_interface_info of name_g : constant is "the name";
begin
  rready <= rvalid;
  process
  begin
    assert rready'x_interface_info = "aximm s0 RREADY" severity failure;
    assert rvalid'x_interface_info = "aximm s0 RVALID" severity failure;
    assert name_g'x_interface_info = "the name" severity failure;
    wait;
  end process;
end architecture;

entity t_vhdl_attrportarch is
end entity;
architecture a of t_vhdl_attrportarch is
  signal r, v : bit := '1';
begin
  u : entity work.t_vhdl_attrportarch_m generic map (name_g => "s0") port map (rready => r, rvalid => v);
  process
  begin
    wait for 1 ns;
    assert r = '1' severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
