-- DESCRIPTION: Verilator: VHDL generic subprogram with a default given by name
--
-- "procedure drive_sig (signal s : out bit) is drive_sig_default" names the
-- subprogram the generic stands for where an instance gives it none, or
-- gives it "open" (6.5.6.2, 6.5.7.1). The name denotes what is visible
-- where the generic is declared: the instance below does not see
-- drive_sig_default, and has a procedure of its own that is not meant
-- (GHDL gna issue2736).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_gensubnamed_pkg is
  procedure drive_sig_default (signal s : out bit);
  procedure drive_sig_zero (signal s : out bit);
end package;

package body t_vhdl_gensubnamed_pkg is
  procedure drive_sig_default (signal s : out bit) is
  begin
    s <= '1';
  end procedure;
  procedure drive_sig_zero (signal s : out bit) is
  begin
    s <= '0';
  end procedure;
end package body;

use work.t_vhdl_gensubnamed_pkg.all;

entity t_vhdl_gensubnamed_gen is
  generic (procedure drive_sig (signal s : out bit) is drive_sig_default);
  port (o : out bit);
end entity;

architecture bhv of t_vhdl_gensubnamed_gen is
begin
  process is
  begin
    drive_sig(o);
    wait;
  end process;
end architecture;

entity t_vhdl_gensubnamed is
end entity;

architecture bhv of t_vhdl_gensubnamed is
  procedure drive_sig_default (signal s : out bit) is
  begin
    s <= '0';
  end procedure;
  signal a, b, c : bit;
begin
  i1 : entity work.t_vhdl_gensubnamed_gen port map (o => a);
  i2 : entity work.t_vhdl_gensubnamed_gen generic map (open) port map (o => b);
  i3 : entity work.t_vhdl_gensubnamed_gen
    generic map (drive_sig => work.t_vhdl_gensubnamed_pkg.drive_sig_zero) port map (o => c);

  process is
  begin
    wait for 1 ns;
    assert a = '1' report "default" severity failure;
    assert b = '1' report "open" severity failure;
    assert c = '0' report "given" severity failure;
    report "gensubnamed done";
    std.env.finish;
  end process;
end architecture;
