-- DESCRIPTION: Verilator: VHDL run-time check: 'DRIVING in a process with no driver (NVC driver7)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_driving_nodrv_bad is
end entity;

architecture a of t_vhdl_driving_nodrv_bad is
  type int_vec is array (natural range <>) of integer;
  function sum (v : int_vec) return integer is
    variable result : integer := 0;
  begin
    for i in v'range loop
      result := result + v(i);
    end loop;
    return result;
  end function;
  subtype rint is sum integer;
  signal x : rint := 0;
  signal y : integer := 0;
begin
  p1 : process is
  begin
    x <= 1;
    y <= 2;
    wait for 1 ns;
    assert x'driving and x'driving_value = 1 severity failure;
    assert y'driving and y'driving_value = 2 severity failure;
    report "p1 done";
    wait;
  end process;

  p2 : process is
  begin
    wait for 2 ns;
    -- p2 has no driver for x: an error (16.2.4)
    assert x'driving severity failure;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
