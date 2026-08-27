-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity sub is
  generic (w : integer := 8);
end entity;

architecture rtl of sub is
  -- A parameter whose length names a generic is a length, not an open
  -- parameter: the instance settles it, so one copy of the function serves
  -- every call rather than one per call site
  function ones (v : bit_vector(w-1 downto 0)) return integer is
    variable n : integer := 0;
  begin
    for i in v'range loop
      if v(i) = '1' then n := n + 1; end if;
    end loop;
    return n;
  end function;

  procedure fill (signal s : out bit_vector(w-1 downto 0); b : bit) is
  begin
    s <= (others => b);
  end procedure;

  signal d : bit_vector(w-1 downto 0) := (others => '0');
begin
  check : process is
    variable v : bit_vector(w-1 downto 0);
  begin
    v := (others => '1');
    assert ones(v) = w report "all ones" severity failure;
    v := (others => '0');
    v(0) := '1';
    assert ones(v) = 1 report "one bit" severity failure;
    fill(d, '1');
    wait for 1 ns;
    assert ones(d) = w report "filled" severity failure;
    report "sub ok";
    wait;
  end process;
end architecture;

-- ---------------------------------------------------------------------------

entity t_vhdl_genlen is
end entity;

architecture rtl of t_vhdl_genlen is
begin
  u4 : entity work.sub generic map (w => 4);
  u12 : entity work.sub generic map (w => 12);

  main : process is
  begin
    wait for 2 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
