-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A signal an instance drives has a driver even though nothing in this
-- architecture assigns it, and every waveform that driver delivers is a
-- transaction on it (14.7.3.4). So 'ACTIVE asked of such an actual answers
-- for the instance's writes, including through a conversion on the formal.

entity t_vhdl_activeinst_child is
  port (q : out integer := 0);
end entity;

architecture rtl of t_vhdl_activeinst_child is
begin
  -- The second waveform carries the value the port already holds: a
  -- transaction and no event
  q <= 1, 1 after 2 ns, 4 after 4 ns;
end architecture;

use std.env.all;

entity t_vhdl_activeinst is
end entity;

architecture tb of t_vhdl_activeinst is
  component t_vhdl_activeinst_child is
    port (q : out integer := 0);
  end component;

  function twice (x : integer) return integer is
  begin
    return x * 2;
  end function;

  signal direct : integer := 0;
  signal conv : integer := 0;
begin

  plain: t_vhdl_activeinst_child port map (q => direct);
  -- A conversion in the formal part converts what the instance writes into
  -- the actual's value (6.5.7.1); the transaction on the actual is the same
  -- one either way.
  --
  -- nvc 1.22.1 disagrees here and re-drives the actual only on an event of
  -- the formal, so it misses the repeated value at 2 ns; GHDL 7.0.0-dev and
  -- Questa 2025.1 both carry the transaction, which is what 6.5.6.3 asks for.
  shaped: t_vhdl_activeinst_child port map (twice(q) => conv);

  main: process is
  begin
    -- Time zero: each instance's driver delivers its first waveform
    wait for 0 ns;
    assert direct'active report "the plain actual saw no transaction at 0 ns"
      severity failure;
    assert conv'active report "the converted actual saw no transaction at 0 ns"
      severity failure;

    wait for 1 ns;
    assert not direct'active report "the plain actual was active between writes"
      severity failure;
    assert not conv'active report "the converted actual was active between writes"
      severity failure;

    -- 2 ns: the same value again, so a transaction and no event
    wait for 1 ns;
    assert direct'active report "the plain actual missed the repeated value"
      severity failure;
    assert conv'active report "the converted actual missed the repeated value"
      severity failure;
    assert not direct'event report "a repeated value was an event" severity failure;

    -- 4 ns: a new value, so both
    wait for 2 ns;
    assert direct'active report "the plain actual missed the new value"
      severity failure;
    assert conv'active report "the converted actual missed the new value"
      severity failure;
    assert direct = 4 report "the plain actual holds " & integer'image(direct)
      severity failure;
    assert conv = 8 report "the converted actual holds " & integer'image(conv)
      severity failure;

    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;
