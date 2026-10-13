-- DESCRIPTION: Verilator: VHDL scalar subtype range checks, within range
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- Values at the ends of their subtypes' ranges, loops over them, and values
-- whose own subtype already fits: none of these stops the run

entity t_vhdl_rangechk is
end entity;

architecture a of t_vhdl_rangechk is
  type my_int is range 1 to 10;
  subtype digit is character range '0' to '9';
  subtype unit_real is real range 0.0 to 1.0;
  subtype small_time is time range 0 ns to 10 ns;
  signal s : my_int := 1;
  signal t : small_time := 0 ns;

  function clamp(x : integer) return natural is
  begin
    if x < 0 then
      return 0;
    end if;
    return x;
  end function;
begin
  process is
    variable v : my_int := 10;
    variable n : natural := 0;
    variable d : digit := '0';
    variable r : unit_real := 0.0;
    variable p : positive;
  begin
    for i in my_int'low to my_int'high loop
      v := i;
      n := n + 1;
    end loop;
    assert v = 10 and n = 10 severity failure;
    d := '9';
    r := 1.0;
    p := clamp(-5) + 1;
    assert p = 1 severity failure;
    s <= 10;
    t <= 10 ns;
    wait for 1 ns;
    assert s = 10 and t = 10 ns severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
