-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity sub is
    generic ( WORD : positive := 8 );
    port ( o : out std_logic_vector(2 * WORD - 1 downto 0) );
end entity;

architecture test of sub is
begin
    -- A slice whose ends move together has a width, however the two are
    -- written: the product spread over the sum is what makes them cancel
    fill : process is
    begin
        for i in 0 to 1 loop
            o((i + 1) * WORD - 1 downto i * WORD) <= (others => '1') when i = 0
                                                     else (others => '0');
        end loop;
        wait;
    end process;
end architecture;

library ieee;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;

entity t_vhdl_slicewidth is
end entity;

architecture test of t_vhdl_slicewidth is
    constant N : natural := 4;
    signal q : std_logic_vector(15 downto 0);
begin
    u : entity work.sub generic map ( WORD => 8 ) port map ( o => q );

    main : process is
        variable v : std_logic_vector(31 downto 0) := (others => '0');
    begin
        for k in 0 to N - 1 loop
            -- Neither end is static, and the two are written differently
            v(8 * (N - k) - 1 downto 8 * (N - 1 - k)) := std_logic_vector(
                to_unsigned(k + 1, 8));
        end loop;
        assert v = X"01020304" report "moving slice: " & to_hstring(v)
            severity failure;

        wait for 1 ns;
        assert q = X"00FF" report "through a generic width: " & to_hstring(q)
            severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
