<perl>return processTemplate("core/object/object/templates/html/admin/body.tpl");</perl>
<h3>Media</h3>
<div>
    <form metod="post" id="updateMedia">
        <table>
            <perl>
                my $cb = sub {
                    my $info    = shift;
                    my $prop_id = $info->{prop_id};
                    if ('title' eq $prop_id){
                        $info->{prop_name} = 'Title';
                        $info->{prop_val} = '<textarea name="title" cols="40">'
                            .$info->{prop_val}.'</textarea>';
                    } elsif ('url' eq $prop_id){
                        $info->{prop_name} = 'URL';
                        $info->{prop_val} = '<input type="text" name="url" size="40"'
                            .' value="'.$info->{prop_val}.'">'
                            .'</input>';
                    }
                };
                my $ret = processProps(
                    props   => [ 'title', 'url' ],
                    cb      => $cb
                );
                return $ret;
            </perl>
            <tr>
                <td>
                    <input type="submit" value="Сохранить"></input>
                </td>
                <td></td>
            </tr>
        </table>
    </form>
</div>
