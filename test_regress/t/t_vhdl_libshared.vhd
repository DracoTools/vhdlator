-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Two libraries, each with a package of the same name declaring an object
-- of the same name. They are different objects (13.5), and each entity
-- reaches only its own.
library liba;

entity t_vhdl_libshared_one is
end entity;

architecture test of t_vhdl_libshared_one is
begin
    main : process is
    begin
        wait for 1 ns;
        liba.obj_pkg.count.bump(1);
        wait;
    end process;
end architecture;

library libb;

entity t_vhdl_libshared_two is
end entity;

architecture test of t_vhdl_libshared_two is
begin
    main : process is
    begin
        wait for 1 ns;
        libb.obj_pkg.count.bump(2);
        libb.obj_pkg.count.bump(20);
        wait;
    end process;
end architecture;

library liba;
library libb;

entity t_vhdl_libshared is
end entity;

architecture test of t_vhdl_libshared is
begin

    u1 : entity work.t_vhdl_libshared_one;
    u2 : entity work.t_vhdl_libshared_two;

    check : process is
    begin
        wait for 2 ns;
        assert liba.obj_pkg.count.get = 1
            report "a " & integer'image(liba.obj_pkg.count.get) severity failure;
        assert libb.obj_pkg.count.get = 22
            report "b " & integer'image(libb.obj_pkg.count.get) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
