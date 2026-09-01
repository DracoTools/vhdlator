-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_dynslicearg_pkg is
    -- A parameter the body writes is handed the caller's own object, so the
    -- call has to say how long it is (4.2.2.3)
    procedure fill (variable v : inout std_logic_vector; constant c : in std_logic);
end package;

package body t_vhdl_dynslicearg_pkg is
    procedure fill (variable v : inout std_logic_vector; constant c : in std_logic) is
    begin
        v := (v'range => c);
    end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_dynslicearg_pkg.all;

entity t_vhdl_dynslicearg_sub is
    generic (GC_W : natural := 4);
    port (o : out std_logic_vector(7 downto 0));
end entity;

architecture test of t_vhdl_dynslicearg_sub is
begin
    main : process is
        variable v_word : std_logic_vector(7 downto 0) := (others => '0');
    begin
        -- The slice is as long as the generic says, which is a length the
        -- instance settles rather than one the text does (8.5)
        fill(v_word(GC_W - 1 downto 0), '1');
        o <= v_word;
        wait;
    end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_dynslicearg is
end entity;

architecture test of t_vhdl_dynslicearg is
    signal o : std_logic_vector(7 downto 0);
begin

    dut : entity work.t_vhdl_dynslicearg_sub
        generic map (GC_W => 4) port map (o => o);

    check : process is
    begin
        wait for 1 ns;
        assert o = "00001111" report "o " & to_string(o) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
