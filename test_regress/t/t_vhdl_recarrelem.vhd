-- DESCRIPTION: Verilator: records in an array aggregate whose element subtype constrains them
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- Each record of "(1 => ("10", 1), ...)" takes its x from f's element
-- subtype, two bits, not from its own value (NVC record34)

entity t_vhdl_recarrelem is
end entity;

architecture a of t_vhdl_recarrelem is
  type rec is record
    x : bit_vector;
    y : natural;
  end record;
  type rec_array is array (natural range <>) of rec;
  type wrapper is record
    f : rec_array(1 to 3)(x(1 to 2));
  end record;
  subtype rarr3 is rec_array(1 to 3)(x(1 to 2));
  signal s : wrapper;
begin
  process is
    variable v : rarr3;
  begin
    s.f(1) <= (x => "10", y => 1);
    wait for 1 ns;
    assert s.f = (1 => ("10", 1), 2 to 3 => ("00", 0)) severity failure;
    assert s.f /= (1 => ("11", 1), 2 to 3 => ("00", 0)) severity failure;
    s.f(2).x(1) <= '1';
    wait for 1 ns;
    assert s.f = (1 => ("10", 1), 2 => ("10", 0), 3 => ("00", 0)) severity failure;
    v := (1 => ("01", 4), others => ("11", 5));
    assert v(3).x = "11" and v(1).y = 4 severity failure;
    s.f <= v;
    wait for 1 ns;
    assert s.f(2).x = "11" and s.f(2).y = 5 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
