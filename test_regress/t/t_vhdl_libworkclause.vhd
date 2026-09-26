-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- "library work;" written out, in a unit analyzed into a library with a name
-- of its own: WORK still denotes that library (13.2). The dependency was
-- recorded on a library literally called "work", and the package was
-- reported missing. UVVM's AXI testbench's slave model writes it so
-- (bitvis_vip_axi/tb/maintenance_tb/axi_slave_model.vhd).
library work;
use work.t_vhdl_libworkclause_pkg.all;

entity t_vhdl_libworkclause is
end entity;

architecture a of t_vhdl_libworkclause is
begin
  process
  begin
    assert K = 5 severity failure;
    assert work.t_vhdl_libworkclause_pkg.K = 5 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
