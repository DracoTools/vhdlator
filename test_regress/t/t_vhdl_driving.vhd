-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- "S'DRIVING_VALUE" is what this process is putting on the signal, which
-- is not what the signal is worth: several drivers are resolved into
-- that, and this one only contributes (16.2.4).  Nor is it what the
-- process has just scheduled -- an assignment is a transaction still to
-- come.  Answers from nvc 1.22.1.

entity t_vhdl_driving is
end entity;

architecture rtl of t_vhdl_driving is

  type int_vec is array (natural range <>) of integer;

  function resolved (v : int_vec) return integer is
    variable result : integer := 0;
  begin
    for i in v'range loop
      result := result + v(i);
    end loop;
    return result;
  end function;

  subtype rint is resolved integer;

  signal x : rint := 0;
  signal z : natural;

begin

  -- Two drivers of one resolved signal, each seeing its own contribution
  p1: process is
  begin
    x <= 1;
    assert x'driving_value = 0 report "p1 before the transaction" severity failure;
    assert x'driving report "p1 not driving" severity failure;
    wait for 1 ns;
    assert x = 3 report "p1 resolved value" severity failure;
    assert x'driving_value = 1 report "p1 own value" severity failure;
    wait;
  end process;

  p2: process is
  begin
    x <= 2;
    wait for 1 ns;
    assert x'driving_value = 2 report "p2 own value" severity failure;
    assert x'driving report "p2 not driving" severity failure;
    wait;
  end process;

  -- A signal nothing resolves has the one driver, and it is the signal
  p3: process is
  begin
    z <= 1;
    assert z = 0 report "p3 before the transaction" severity failure;
    assert z'driving_value = 0 report "p3 driving value before" severity failure;
    wait for 1 ns;
    assert z'driving_value = 1 report "p3 driving value after" severity failure;
    assert z'driving report "p3 not driving" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
