-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_protshared_pkg is
    -- What a protected type holds is reached only through its own methods
    -- (5.6), and one object of it is one lot of state however many places
    -- call into it
    type t_counter is protected
        procedure bump (n : integer);
        impure function get return integer;
    end protected;
end package;

package body t_vhdl_protshared_pkg is
    type t_counter is protected body
        variable priv_count : integer := 0;

        procedure bump (n : integer) is
        begin
            priv_count := priv_count + n;
        end procedure;

        impure function get return integer is
        begin
            return priv_count;
        end function;
    end protected body;
end package body;

use work.t_vhdl_protshared_pkg.all;

package t_vhdl_protshared_shared is
    -- A shared variable a package declares is one object for the whole
    -- design, not one per module that names it (6.4.2.4)
    shared variable counter : t_counter;
end package;

use work.t_vhdl_protshared_pkg.all;
use work.t_vhdl_protshared_shared.all;

entity t_vhdl_protshared_one is
end entity;

architecture test of t_vhdl_protshared_one is
begin
    main : process is
    begin
        wait for 1 ns;
        counter.bump(1);
        wait;
    end process;
end architecture;

use work.t_vhdl_protshared_pkg.all;
use work.t_vhdl_protshared_shared.all;

entity t_vhdl_protshared_two is
end entity;

architecture test of t_vhdl_protshared_two is
begin
    main : process is
    begin
        wait for 2 ns;
        counter.bump(10);
        wait;
    end process;
end architecture;

use work.t_vhdl_protshared_pkg.all;
use work.t_vhdl_protshared_shared.all;

entity t_vhdl_protshared is
end entity;

architecture test of t_vhdl_protshared is
begin

    u1 : entity work.t_vhdl_protshared_one;
    u2 : entity work.t_vhdl_protshared_two;

    check : process is
    begin
        wait for 3 ns;
        -- Both entities counted into the one object
        assert counter.get = 11
            report "count " & integer'image(counter.get) severity failure;
        counter.bump(100);
        assert counter.get = 111 report "after" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
