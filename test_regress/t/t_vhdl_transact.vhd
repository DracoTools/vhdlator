-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_transact is
end entity;

architecture a of t_vhdl_transact is
  signal x : integer;
  signal t : bit_vector(1 to 6);
begin

  -- Three transactions, at 0, 2 ns and 4 ns
  x <= 1, 2 after 2 ns, 3 after 4 ns;

  watch : process is
  begin
    for i in 1 to 6 loop
      t(i) <= x'transaction;
      wait for 1 ns;
    end loop;
    wait;
  end process;

  main : process is
  begin
    wait for 7 ns;
    -- 16.2.4 leaves the initial value undefined; nvc 1.22.1 and GHDL
    -- 7.0.0-dev both start it at '1' and toggle it as each element of the
    -- waveform is delivered, which is what these six samples record.
    assert t = "101100" report "transaction bits are " & to_string(t) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
