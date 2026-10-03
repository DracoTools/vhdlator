-- DESCRIPTION: Verilator: VHDL external name whose carets climb out of blocks and a generate
--
-- Each "^" leaves one enclosing region: from a block inside an iteration
-- of a generate, "^.^" is the architecture again, and "g(1).y" names the
-- signal that iteration declares (8.7).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_extnamecaret is
end entity;

architecture test of t_vhdl_extnamecaret is
begin

    g : for i in 1 to 1 generate
        signal y : integer := 0;
    begin
        b : block is
            signal x : integer := 0;
        begin
            process is
            begin
                << signal ^.^.g(1).y : integer >> <= 55;
                wait for 1 ns;
                assert x = 42 severity failure;
                wait;
            end process;
        end block;

        process is
        begin
            << signal b.x : integer >> <= 42;
            wait for 1 ns;
            assert y = 55 severity failure;
            wait;
        end process;
    end generate;

    process is
    begin
        wait for 2 ns;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
