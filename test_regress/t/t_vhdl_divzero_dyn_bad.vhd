-- DESCRIPTION: Verilator: VHDL run-time check: an INTEGER divided by a zero only running gives
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_divzero_dyn_bad is
end entity;

architecture a of t_vhdl_divzero_dyn_bad is
  signal s : integer := 0;
  function noret(x : integer) return integer is
  begin
    if x > 0 then
      return x;
    end if;
  end function;
begin
  process is
    variable i : integer := 2;
    variable n : integer := 1;
  begin
    i := 10 / i;
    i := 10 mod i + 10 rem i;
    report "five fits";
    i := 0;
    i := 10 / i;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
