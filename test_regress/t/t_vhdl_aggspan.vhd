-- DESCRIPTION: Verilator: VHDL aggregates whose bounds are not numbers here
--
-- "(i*8+7 downto i*8 => '1')" has ends only running settles but is always
-- eight long, so it is a value of fixed width, as the slice it is written
-- into is (9.3.3.3). An aggregate with named choices of an array whose index
-- subtype has a bound a generic gives says where each element goes on its
-- own (9.3.3.3).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aggspan is
    generic ( N : integer := 2 );
end entity;

architecture test of t_vhdl_aggspan is
    subtype t_two is integer range 1 to N;
    type t_table is array (t_two, t_two) of boolean;
    constant table : t_table := (1 => (true, false), 2 => (false, true));
    type t_row is array (t_two) of boolean;
    constant row : t_row := (1 => true, 2 => false);
    type t_down is array (N downto 1) of bit;
    constant down : t_down := (2 => '1', 1 => '0');
begin

    process is
        variable v : bit_vector(31 downto 0) := (others => '0');
        variable u : bit_vector(0 to 15) := (others => '0');
        variable i : integer := 3;
    begin
        v(i*8+7 downto i*8) := (i*8+7 downto i*8 => '1');
        assert v = X"FF000000" severity failure;
        i := 1;
        v(i*8+3 downto i*8) := (i*8+3 downto i*8 => '1');
        assert v = X"FF000F00" severity failure;
        u(i*4 to i*4+3) := (i*4 to i*4+3 => '1');
        assert u = X"0F00" severity failure;

        assert table(1, 1) and not table(1, 2) severity failure;
        assert not table(2, 1) and table(2, 2) severity failure;
        assert row(1) and not row(2) severity failure;
        assert down = "10" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
