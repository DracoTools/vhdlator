-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_sigelem is
end entity;

-- 4.2.2.3 asks that the actual for a signal parameter be a *static* signal
-- name, and both nvc 1.22.1 and GHDL 7.0.0-dev refuse "s(i)" for it unless
-- told to relax the rule. Real designs write it, so it is accepted here, as
-- it is by "nvc -a --relaxed", which this file was checked against.

architecture a of t_vhdl_sigelem is

  procedure set_bit(signal x : out bit; constant v : in bit) is
  begin
    x <= v;
  end procedure;

  procedure clear_and_wait(signal x : out bit) is
  begin
    x <= '0';
    wait for 1 ns;
  end procedure;

  signal s : bit_vector(1 to 7) := (others => '0');
  signal t : bit_vector(0 to 3) := (others => '1');

begin

  main : process
  begin
    -- The formal is one element of an array signal, and which element only
    -- the call knows: the index is a loop parameter
    for i in s'range loop
      set_bit(s(i), '1');
    end loop;
    wait for 1 ns;
    assert s = "1111111" report "s is not all ones" severity failure;

    -- and again where the body waits, so the copy is a suspendable task
    for i in t'range loop
      clear_and_wait(t(i));
    end loop;
    assert t = "0000" report "t is not all zeros" severity failure;

    -- one written back, at a computed index
    set_bit(s(2 + 3), '0');
    wait for 1 ns;
    assert s = "1111011" report "s(5) was not cleared" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
