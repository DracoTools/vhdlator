-- DESCRIPTION: Verilator: VHDL signal driven only by an out port starts at that port's driver
--
-- "q => z" with q an out port of no default: z's one source is q's
-- driver, which starts at its subtype's left, so z starts there too and
-- not at its own declared default (14.7.3.2, 6.5.6.3). With its own, it
-- changed at delta 1 although nothing did, or failed to change where the
-- references see an event (GHDL gna issue2536).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_outportinit_pkg is
  type tri is (N, O, P);
  type tri_vec is array (natural range <>) of tri;
end package;

use work.t_vhdl_outportinit_pkg.all;

entity t_vhdl_outportinit_sub is
  generic (w : natural);
  port (i : in tri_vec(w-1 downto 0);
        q : out tri_vec(w-1 downto 0);
        co : out tri);
end entity;

architecture a of t_vhdl_outportinit_sub is
begin
  q <= i;
  co <= O;
end architecture;

use work.t_vhdl_outportinit_pkg.all;

entity t_vhdl_outportinit is
end entity;

architecture a of t_vhdl_outportinit is
  signal x : tri_vec(2 downto 0) := (others => O);
  signal z : tri_vec(2 downto 0) := (others => O);
  signal d : tri := O;
  signal events : natural := 0;
begin
  u : entity work.t_vhdl_outportinit_sub generic map (w => 3) port map (i => x, q => z, co => d);

  process (z, d)
  begin
    report "z=" & tri'image(z(0)) & tri'image(z(2)) & " d=" & tri'image(d);
    if z'event then
      events <= events + 1;
    end if;
  end process;

  process
  begin
    assert z = (N, N, N) and d = N severity failure;
    wait for 1 ns;
    assert z = (O, O, O) and d = O severity failure;
    assert events = 1 severity failure;
    report "outportinit done";
    std.env.finish;
  end process;
end architecture;
